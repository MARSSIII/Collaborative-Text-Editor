#include "collab/token_bucket.h"

namespace collab {

TokenBucket::TokenBucket(uint32_t capacity)
    : tokens_(capacity), capacity_(capacity) {}

bool TokenBucket::try_consume(uint32_t count) noexcept {
    uint64_t current = tokens_.load(std::memory_order_relaxed);
    while (true) {
        if (current < count) {
            return false;
        }
        if (tokens_.compare_exchange_weak(current, current - count,
                                          std::memory_order_relaxed)) {
            return true;
        }
    }
}

void TokenBucket::refill() noexcept {
    tokens_.store(capacity_, std::memory_order_relaxed);
}

void TokenBucket::advance_time(std::chrono::milliseconds duration) {
    uint64_t tokens_to_add = capacity_ * static_cast<uint64_t>(duration.count()) / 1000;
    if (tokens_to_add == 0 && duration.count() > 0) {
        tokens_to_add = 1;
    }
    uint64_t current = tokens_.load(std::memory_order_relaxed);
    while (true) {
        uint64_t new_val = std::min(current + tokens_to_add, capacity_);

        if (tokens_.compare_exchange_weak(current, new_val,
                                          std::memory_order_relaxed)) {
            return;
        }
    }
}

}
