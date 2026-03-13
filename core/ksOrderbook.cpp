#include "ksOrderbook.h"
#include <chrono>
#include <iostream>

Orderbook::Orderbook(const std::string& market_ticker, const std::string& market_id) : market_ticker_(market_ticker), market_id_(market_id)
{
    logger_ = new KsLogger("logs/" + market_ticker_ + "_replay.log", true);
}

void Orderbook::attachShm(ShmOrderbook* slot) {
    shm_book_ = slot;
}

void Orderbook::publishDeltaToShm(Price price, Quantity quantity, YesNo yesno) {
    if (!shm_book_) return;

    // This is the only writer (single threaded), no spinlock needed. -andrew

    ShmBookSide& d = (yesno == YesNo::YES) ? shm_book_->yes : shm_book_->no;
    ShmBookSide& o = (yesno == YesNo::YES) ? shm_book_->no : shm_book_->yes;

    d.bids[price].seq.fetch_add(1, std::memory_order_acq_rel); // lock
    o.asks[100 - price].seq.fetch_add(1, std::memory_order_acq_rel); // lock

    d.bids[price].last_update_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    o.asks[100 - price].last_update_ns = d.bids[price].last_update_ns;

    d.bids[price].quantity += quantity;
    o.asks[100 - price].quantity += quantity;

    std::cout << "Published delta to SHM: " << (yesno == YesNo::YES ? "YES" : "NO") << " price=" << price << " quantity=" << d.bids[price].quantity << std::endl;

    d.bids[price].seq.fetch_add(1, std::memory_order_acq_rel); // unlock
    o.asks[100 - price].seq.fetch_add(1, std::memory_order_acq_rel); // unlock
}

void Orderbook::publishSnapshotToShm(const ksApiMessage& snapshot) {
    if (!shm_book_) return;

    // This is the only writer (single threaded), no spinlock needed. -andrew

    ShmBookSide& yes = shm_book_->yes;
    ShmBookSide& no = shm_book_->no;

    for (const auto& entry : snapshot["msg"]["yes_dollars_fp"]) {
        double price_double = std::stod(entry[0].get<std::string>());
        Price price = static_cast<Price>(price_double * 100);
        Quantity qty = static_cast<Quantity>(std::stod(entry[1].get<std::string>()));
        yes.bids[price].seq.fetch_add(1, std::memory_order_acq_rel); // lock
        no.asks[100 - price].seq.fetch_add(1, std::memory_order_acq_rel); // lock
        uint64_t now_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        yes.bids[price].last_update_ns = now_ns;
        no.asks[100 - price].last_update_ns = now_ns;
        yes.bids[price].quantity += qty;
        no.asks[100 - price].quantity += qty;
        yes.bids[price].seq.fetch_add(1, std::memory_order_acq_rel); // unlock
        no.asks[100 - price].seq.fetch_add(1, std::memory_order_acq_rel); // unlock
    }

    for (const auto& entry : snapshot["msg"]["no_dollars_fp"]) {
        double price_double = std::stod(entry[0].get<std::string>());
        Price price = static_cast<Price>(price_double * 100);
        Quantity qty = static_cast<Quantity>(std::stod(entry[1].get<std::string>()));
        no.bids[price].seq.fetch_add(1, std::memory_order_acq_rel); // lock
        yes.asks[100 - price].seq.fetch_add(1, std::memory_order_acq_rel); // lock
        uint64_t now_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        no.bids[price].last_update_ns = now_ns;
        yes.asks[100 - price].last_update_ns = now_ns;
        no.bids[price].quantity += qty;
        yes.asks[100 - price].quantity += qty;
        no.bids[price].seq.fetch_add(1, std::memory_order_acq_rel); // unlock
        yes.asks[100 - price].seq.fetch_add(1, std::memory_order_acq_rel); // unlock
    }
}

void Orderbook::ingestSnapshot(const ksApiMessage& snapshot) {
    logger_->info(snapshot.dump());

    publishSnapshotToShm(snapshot);
}

void Orderbook::ingestDelta(const ksApiMessage& delta) {
    logger_->info(delta.dump());
    double price_double = std::stod(delta["msg"]["price_dollars"].get<std::string>());
    Price price = static_cast<Price>(price_double * 100);
    QuantityDelta q_delta = static_cast<QuantityDelta>(std::stod(delta["msg"]["delta_fp"].get<std::string>()));
    YesNo side = delta["msg"]["side"] == "yes" ? YesNo::YES : YesNo::NO;

    publishDeltaToShm(price, q_delta, side);

}