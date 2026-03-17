# CLAUDE.md — ksOrderbook2

## Project overview

C++20 WebSocket subscriber that streams real-time orderbook data from the
[Kalshi](https://kalshi.com) prediction-markets exchange. It:

1. Opens a TLS WebSocket to the Kalshi API and authenticates with RSA-PSS signed
   request headers.
2. Subscribes to channels (orderbook snapshots + deltas) for a list of market
   tickers.
3. Maintains a live in-memory orderbook per market (`Orderbook`, unified under
   `UnifiedBook`).
4. Publishes current orderbook state to a POSIX shared-memory segment using a
   seqlock so that separate strategy processes can read with minimal latency and
   zero copying.
5. Persists every raw WebSocket message to a per-ticker replay log for
   backtesting.

---

## Repository layout

```
ksOrderbook2/
├── CLAUDE.md                  ← this file
├── Makefile
├── README.md
├── cfg/
│   ├── subscribe_auth.toml    ← auth credentials (fill in before running)
│   └── climate_subscribe_NY.toml   ← example subscription config
├── ksSubscriber.h / .cpp      ← top-level Subscriber class + main()
└── core/
    ├── ksTypes.h              ← shared typedefs and enums
    ├── ksWebsocket.h / .cpp   ← TLS WebSocket client (Boost.Beast)
    ├── ksSecretKey.h / .cpp   ← RSA-PSS signing (OpenSSL EVP)
    ├── ksOrderbook.h / .cpp   ← per-market orderbook; ingest snapshots & deltas
    ├── ksUnifiedBook.h / .cpp ← map of ticker → Orderbook + ShmWriter owner
    ├── ksShm.h                ← shared-memory layout + ShmWriter
    └── ksLogger.h / .cpp      ← timestamp-prefixed file logger
```

---

## Key classes and data structures

### `Subscriber` (`ksSubscriber.h`)
Top-level orchestrator. Holds a `WebsocketClient`, a `UnifiedBook`, and the
message-routing loop. Call `subscribeNewFeed()` to send a subscription request,
then `listenLoop()` / `listenAsync()` to process incoming messages.

### `WebsocketClient` (`core/ksWebsocket.h`)
Wraps Boost.Asio + Boost.Beast. Handles TLS handshake, HTTP upgrade, reading
frames, and sending JSON messages. Supports both synchronous (`readMessageFromWebSocket`) 
and async (`startAsyncRead` + `run`) operation.

### `SecretKey` (`core/ksSecretKey.h`)
Loads a PEM private key from disk and exposes `sign(prehash)` which returns a
Base64-encoded RSA-PSS/SHA-256 signature used as the `KALSHI-ACCESS-SIGNATURE`
header.

### `UnifiedBook` (`core/ksUnifiedBook.h`)
Owns a `BookMap` (`std::map<std::string, Orderbook>`) keyed by market ticker and
a `ShmWriter`. Delegates snapshot/delta ingestion to the correct `Orderbook`.
Call `initShm()` before subscribing.

### `Orderbook` (`core/ksOrderbook.h`)
Holds two `MarketSide` structs (`yes_` / `no_`). Each `MarketSide` keeps
`BidLevels` (price → quantity, sorted descending) and `AskLevels` (ascending).

YES/NO prices use a unified 1–99 cent scale:
- YES prices are stored directly.
- NO prices are stored as complements: `stored_price = 100 - api_price`.

### Shared-memory layout (`core/ksShm.h`)
```
ShmLayout {
    atomic<uint32_t> num_active;         // number of active markets
    char tickers[64][64];                // ticker strings
    ShmOrderbook books[64] {             // one per market
        ShmBookSide yes {
            ShmLevel bids[99];           // indexed by price cent (1–99)
            ShmLevel asks[99];
        };
        ShmBookSide no { … };
        char market_ticker[64];
    };
}
```
Each `ShmLevel` uses a seqlock (`atomic<uint64_t> seq`) so readers can detect
torn reads without a mutex.

### `KsLogger` (`core/ksLogger.h`)
Simple file logger. Each `Orderbook` uses one to write all raw API messages to
`logs/<ticker>_replay.log`.

---

## Types (`core/ksTypes.h`)

| Type / Enum | Definition |
|---|---|
| `Headers` | `std::map<std::string, std::string>` |
| `ksApiMessage` | `nlohmann::json` |
| `ksConfigSettings` | `nlohmann::json` |
| `Price` | `uint32_t` (cents, 1–99) |
| `Quantity` | `uint32_t` |
| `QuantityDelta` | `int32_t` |
| `WebsocketMsgType` | enum: `orderbook_snapshot`, `orderbook_delta`, `ticker`, … |
| `Side` | `BID` / `ASK` |
| `YesNo` | `YES` / `NO` |

---

## Build

**Dependencies:** g++ ≥ 11, Boost.Asio/Beast, OpenSSL, nlohmann/json, cpptoml.

```bash
make all    # produces ./subscriber
make clean  # removes build/ and ./subscriber
```

Build flags: `-std=c++20 -Wall -Wextra -O2`

Link flags: `-lboost_system -lpthread -lssl -lcrypto -lrt`

---

## Configuration files

### `cfg/subscribe_auth.toml`
```toml
URL           = "wss://api.elections.kalshi.com"
PATH          = "/trade-api/ws/v2"
API_KEY       = "<your Kalshi API key>"
SIGN_KEY_PATH = "<path to PEM private key>"
```

### `cfg/<name>.toml` (subscription config)
```toml
SUBSCRIBE_TICKERS = ["TICKER-A", "TICKER-B"]
SHM_NAME          = "ks_shm_<name>"   # optional; default: /ks_orderbooks
```

---

## Running

```bash
./subscriber -a cfg/subscribe_auth.toml -s cfg/climate_subscribe_NY.toml
```

The subscriber will:
1. Connect and authenticate.
2. Subscribe to `orderbook_delta` and `orderbook_snapshot` for each ticker.
3. Write orderbook state to `/dev/shm/<SHM_NAME>`.
4. Append raw messages to `logs/<ticker>_replay.log`.

---

## Shared-memory consumer notes (for strategy processes)

- Open: `shm_open(<SHM_NAME>, O_RDONLY, 0)` then `mmap` to `ShmLayout`.
- Find market index with `num_active` and the `tickers` array.
- Read a level with seqlock: spin until `seq` is even before reading, then
  re-check `seq` hasn't changed (retry on odd or changed value).
- Price index = cent value - 1 (i.e., 50 cents → index 49).
- NO prices stored as complement: `stored_index = (100 - api_no_price) - 1`.

---

## Coding conventions

- C++20 throughout; headers use `#pragma once` or include guards.
- JSON parsing via `nlohmann::json` (`ksApiMessage` / `ksConfigSettings`).
- TOML config via `cpptoml`.
- All `std::string` passed by `const&`; large objects owned by `std::unique_ptr`.
- No exceptions are caught in hot paths; errors propagate via `std::runtime_error`.
- Shared-memory writes use `std::memory_order_release`; reads should use
  `std::memory_order_acquire`.
