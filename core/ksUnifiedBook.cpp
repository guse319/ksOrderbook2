#include "ksUnifiedBook.h"
#include "ksOrderbook.h"

void UnifiedBook::initShm(const std::string& shm_name) {
    shm_ = std::make_unique<ShmWriter>(shm_name);
}

void UnifiedBook::initNewBook(const ksApiMessage& snapshot) {
    std::string market_ticker = snapshot["msg"]["market_ticker"];
    std::string market_id = snapshot["msg"]["market_id"];

    // Insert a default-constructed book then fetch a stable reference
    books_.emplace(std::piecewise_construct,
                   std::forward_as_tuple(market_ticker),
                   std::forward_as_tuple(market_ticker, market_id));
    Orderbook& book = books_.at(market_ticker);

    // Attach shared memory slot before ingesting so the snapshot is published
    if (shm_) {
        ShmOrderbook* slot = shm_->allocateSlot(market_ticker);
        book.attachShm(slot);
    }

    book.ingestSnapshot(snapshot);
}

void UnifiedBook::updateBookWithDelta(const ksApiMessage& delta) {
    std::string market_ticker = delta["msg"]["market_ticker"];
    auto it = books_.find(market_ticker);
    if (it != books_.end()) {
        it->second.ingestDelta(delta);
    } else {
        throw std::runtime_error("Orderbook for market_ticker " + market_ticker + " not found.");
    }
}