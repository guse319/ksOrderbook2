// ksShm.h
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Shared memory layout constants
constexpr size_t SHM_MAX_MARKETS = 64;
constexpr size_t SHM_MAX_LEVELS  = 99; // This is currently truncating price levels to penny, but kalshi is soon launching sub-penny ticks.
constexpr size_t SHM_TICKER_LEN  = 64;
constexpr char   SHM_DEFAULT_NAME[] = "/ks_orderbooks";

// Shared memory objects
struct ShmLevel {
    std::atomic<uint64_t> seq{0}; // for seqlock
    uint64_t last_update_ns{0};
    uint32_t quantity{0};
};

struct ShmBookSide {
    ShmLevel bids[SHM_MAX_LEVELS];
    ShmLevel asks[SHM_MAX_LEVELS];
};

struct ShmOrderbook {
    ShmBookSide yes;
    ShmBookSide no;
    char market_ticker[SHM_TICKER_LEN];
};

struct ShmLayout {
    std::atomic<uint32_t> num_active{0};
    char tickers[SHM_MAX_MARKETS][SHM_TICKER_LEN];
    ShmOrderbook books[SHM_MAX_MARKETS];
};

class ShmWriter {
public:
    explicit ShmWriter(const std::string& name) : name_(name) {
        int fd = shm_open(name_.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
            throw std::runtime_error("ShmWriter: shm_open failed for " + name_);

        if (ftruncate(fd, sizeof(ShmLayout)) == -1) {
            close(fd);
            throw std::runtime_error("ShmWriter: ftruncate failed");
        }

        void* ptr = mmap(nullptr, sizeof(ShmLayout), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (ptr == MAP_FAILED)
            throw std::runtime_error("ShmWriter: mmap failed");

        layout_ = new (ptr) ShmLayout();
    }

    ~ShmWriter() {
        if (layout_) {
            munmap(layout_, sizeof(ShmLayout));
            shm_unlink(name_.c_str());
        }
    }

    ShmWriter(const ShmWriter&) = delete;
    ShmWriter& operator=(const ShmWriter&) = delete;

    // Appends a new slot for ticker and returns a pointer to it.
    ShmOrderbook* allocateSlot(const std::string& ticker) {
        uint32_t idx = layout_->num_active.load(std::memory_order_relaxed);
        if (idx >= SHM_MAX_MARKETS)
            throw std::runtime_error("ShmWriter: exceeded SHM_MAX_MARKETS");

        std::strncpy(layout_->tickers[idx], ticker.c_str(), SHM_TICKER_LEN - 1);
        layout_->tickers[idx][SHM_TICKER_LEN - 1] = '\0';

        ShmOrderbook& book = layout_->books[idx];
        std::strncpy(book.market_ticker, ticker.c_str(), SHM_TICKER_LEN - 1);
        book.market_ticker[SHM_TICKER_LEN - 1] = '\0';

        layout_->num_active.store(idx + 1, std::memory_order_release);
        return &book;
    }

private:
    std::string name_;
    ShmLayout*  layout_ = nullptr;
};

// class ShmReader {
// public:
//     ShmReader() = default;

//     ~ShmReader() {
//         if (layout_) munmap(layout_, sizeof(ShmLayout));
//     }

//     ShmReader(const ShmReader&) = delete;
//     ShmReader& operator=(const ShmReader&) = delete;

//     bool open(const std::string& name) {
//         int fd = shm_open(name.c_str(), O_RDONLY, 0);
//         if (fd == -1) return false;

//         void* ptr = mmap(nullptr, sizeof(ShmLayout), PROT_READ, MAP_SHARED, fd, 0);
//         close(fd);
//         if (ptr == MAP_FAILED) return false;

//         layout_ = static_cast<ShmLayout*>(ptr);
//         return true;
//     }

//     int findSlot(const std::string& ticker) const {
//         uint32_t n = layout_->num_active.load(std::memory_order_acquire);
//         for (uint32_t i = 0; i < n; ++i) {
//             if (std::strncmp(layout_->tickers[i], ticker.c_str(), SHM_TICKER_LEN) == 0)
//                 return static_cast<int>(i);
//         }
//         return -1;
//     }

//     void readBook(int slot, ShmBookData& out) const {
//         const ShmOrderbook& src = layout_->books[slot];
//     }

// private:
//     ShmLayout* layout_ = nullptr;
// };
