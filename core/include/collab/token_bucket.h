#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

namespace collab {

class TokenBucket {
public:
    explicit TokenBucket(uint32_t capacity);

    bool try_consume(uint32_t count = 1) noexcept;
    void refill() noexcept;
    void advance_time(std::chrono::milliseconds duration);

private:
    std::atomic<uint64_t> tokens_;
    uint64_t capacity_;
};

}
