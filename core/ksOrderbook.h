#ifndef KS_ORDERBOOK_H
#define KS_ORDERBOOK_H

#include "ksTypes.h"
#include "core/ksLogger.h"
#include "core/ksShm.h"
#include <string>
#include <map>
#include <vector>

class Orderbook {
public:
    Orderbook(const std::string& market_ticker, const std::string& market_id);
    ~Orderbook() = default;

    void ingestSnapshot(const ksApiMessage& snapshot);
    void ingestDelta(const ksApiMessage& delta);

    void attachShm(ShmOrderbook* slot);

private:
    void publishDeltaToShm(Price price, Quantity quantity, YesNo yesno);
    void publishSnapshotToShm(const ksApiMessage& snapshot);

    std::string market_ticker_;
    std::string market_id_;
    KsLogger* logger_ = nullptr;
    ShmOrderbook* shm_book_ = nullptr;
};

#endif // KS_ORDERBOOK_H
