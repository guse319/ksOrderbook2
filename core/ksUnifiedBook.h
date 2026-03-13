#ifndef KS_UNIFIEDBOOK_H
#define KS_UNIFIEDBOOK_H

#include "ksTypes.h"
#include "ksOrderbook.h"
#include "ksShm.h"
#include <memory>
#include <string>
#include <map>
#include <vector>

using BookMap = std::map<std::string, Orderbook>;

class UnifiedBook {

public:

    void initShm(const std::string& shm_name);

    void initNewBook(const ksApiMessage& snapshot);

    void updateBookWithDelta(const ksApiMessage& delta);

private:
    BookMap                  books_;
    std::unique_ptr<ShmWriter> shm_;

};


#endif // KS_UNIFIEDBOOK_H