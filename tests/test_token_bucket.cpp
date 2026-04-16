#include <gtest/gtest.h>
#include "collab/token_bucket.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace collab;

TEST(TokenBucket, AllowsWithinLimit) {
    TokenBucket bucket(10);

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(bucket.try_consume(1)) << "Failed at token " << i;
    }
}

TEST(TokenBucket, RejectsOverLimit) {
    TokenBucket bucket(5);

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(bucket.try_consume(1));
    }

    EXPECT_FALSE(bucket.try_consume(1));
    EXPECT_FALSE(bucket.try_consume(1));
}

TEST(TokenBucket, RefillsAfterTime) {
    TokenBucket bucket(5);

    for (int i = 0; i < 5; ++i) {
        bucket.try_consume(1);
    }
    EXPECT_FALSE(bucket.try_consume(1));

    bucket.advance_time(std::chrono::milliseconds(1000));

    EXPECT_TRUE(bucket.try_consume(1));
    EXPECT_TRUE(bucket.try_consume(1));
}

TEST(TokenBucket, PartialRefill) {
    TokenBucket bucket(10);

    for (int i = 0; i < 10; ++i) {
        bucket.try_consume(1);
    }
    EXPECT_FALSE(bucket.try_consume(1));

    bucket.advance_time(std::chrono::milliseconds(500));

    int available = 0;
    while (bucket.try_consume(1)) {
        ++available;
    }
    EXPECT_EQ(available, 5);
}

TEST(TokenBucket, ConcurrentAccess) {
    constexpr int num_threads = 8;
    constexpr int attempts_per_thread = 50;
    constexpr uint32_t capacity = 100;

    TokenBucket bucket(capacity);

    std::atomic<int> allowed{0};
    std::atomic<int> rejected{0};

    std::vector<std::jthread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < attempts_per_thread; ++i) {
                if (bucket.try_consume(1)) {
                    allowed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    rejected.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    threads.clear();

    EXPECT_EQ(allowed.load(), static_cast<int>(capacity));
    EXPECT_EQ(rejected.load(), num_threads * attempts_per_thread - static_cast<int>(capacity));
}
