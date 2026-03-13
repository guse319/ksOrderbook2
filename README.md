# Kalshi Subscriber

A C++17 WebSocket client that streams real-time orderbook data from the Kalshi prediction markets exchange. It connects over TLS, authenticates using RSA-PSS signed request headers, and maintains a live in-memory orderbook for each subscribed market — processing both full snapshots and incremental delta updates as they arrive.

Market data is written to a POSIX shared memory segment using a seqlock scheme, so that separate strategy processes can read current orderbook state with minimal latency and no copying overhead. Each market's full message stream is also persisted to a replay log for backtesting.

## Building

Requires: g++, Boost.Asio/Beast, OpenSSL, nlohmann/json, cpptoml.

```bash
make all    # builds ./subscriber
make clean  # removes build artifacts
```

## Running

```bash
./subscriber -a <auth_config.toml> -s <subscription_config.toml>
```

Example:
```bash
./subscriber -a cfg/subscribe_auth.toml -s cfg/climate_subscribe_NY.toml
```

**Auth config fields:** `URL`, `PATH`, `API_KEY`, `SIGN_KEY_PATH`

**Subscription config fields:** `SUBSCRIBE_TICKERS` (array of market ticker strings), `SHM_NAME` (optional, default: `/ks_orderbooks`)

## Output

- **Shared memory:** orderbook state published to `/dev/shm/<SHM_NAME>` via seqlock for low-latency reads by strategy processes
- **Replay logs:** full message stream written to `logs/<ticker>_replay.log`

## Architecture

```
Subscriber
├── WebsocketClient     — TLS WebSocket connect/read/write (Boost.Beast)
├── UnifiedBook         — map of ticker → Orderbook
│     └── Orderbook     — per-market YES/NO bid/ask levels; ingests snapshots & deltas
├── ShmWriter           — publishes orderbook state to POSIX shared memory
└── KsLogger            — timestamp-prefixed file logger for replay logs
```

Authentication uses RSA-PSS/SHA-256 signatures (OpenSSL EVP API) injected into the WebSocket HTTP upgrade request. YES/NO prices are stored on a unified 1–99 cent scale, with NO prices represented as complements (`100 - price`).
