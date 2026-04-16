#include <gtest/gtest.h>
#include "collab/mpsc_queue.h"

#include <memory>
#include <set>
#include <thread>
#include <vector>

using namespace collab;

TEST(MPSCQueue, SingleProducerSingleConsumer) {
    MPSCQueue<int> queue;

    queue.enqueue(1);
    queue.enqueue(2);
    queue.enqueue(3);

    auto v1 = queue.try_dequeue();
    auto v2 = queue.try_dequeue();
    auto v3 = queue.try_dequeue();

    ASSERT_TRUE(v1.has_value());
    ASSERT_TRUE(v2.has_value());
    ASSERT_TRUE(v3.has_value());

    EXPECT_EQ(*v1, 1);
    EXPECT_EQ(*v2, 2);
    EXPECT_EQ(*v3, 3);
}

TEST(MPSCQueue, EmptyQueueReturnsNullopt) {
    MPSCQueue<int> queue;

    auto result = queue.try_dequeue();
    EXPECT_FALSE(result.has_value());
}

TEST(MPSCQueue, ConcurrentProducersNoLoss) {
    constexpr int num_producers = 8;
    constexpr int items_per_producer = 10000;
    constexpr int total_items = num_producers * items_per_producer;

    MPSCQueue<int> queue;

    std::vector<std::jthread> producers;
    producers.reserve(num_producers);

    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                queue.enqueue(p * items_per_producer + i);
            }
        });
    }

    producers.clear();

    std::set<int> received;
    while (auto val = queue.try_dequeue()) {
        received.insert(*val);
    }

    EXPECT_EQ(static_cast<int>(received.size()), total_items)
        << "Lost " << (total_items - received.size()) << " items";

    for (int i = 0; i < total_items; ++i) {
        EXPECT_TRUE(received.contains(i)) << "Missing item: " << i;
    }
}

TEST(MPSCQueue, ConcurrentProducerAndConsumer) {
    constexpr int num_items = 50000;

    MPSCQueue<int> queue;
    std::vector<int> consumed;
    consumed.reserve(num_items);

    std::jthread consumer([&]() {
        int count = 0;
        while (count < num_items) {
            if (auto val = queue.try_dequeue()) {
                consumed.push_back(*val);
                ++count;
            }
        }
    });

    std::jthread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            queue.enqueue(i);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(static_cast<int>(consumed.size()), num_items);
    for (size_t i = 0; i < static_cast<size_t>(num_items); ++i) {
        EXPECT_EQ(consumed[i], static_cast<int>(i)) << "Out-of-order at index " << i;
    }
}

TEST(MPSCQueue, MoveOnlyType) {
    MPSCQueue<std::unique_ptr<int>> queue;

    queue.enqueue(std::make_unique<int>(42));
    queue.enqueue(std::make_unique<int>(99));

    auto v1 = queue.try_dequeue();
    auto v2 = queue.try_dequeue();
    auto v3 = queue.try_dequeue();

    ASSERT_TRUE(v1.has_value());
    ASSERT_TRUE(v2.has_value());
    EXPECT_FALSE(v3.has_value());

    EXPECT_EQ(**v1, 42);
    EXPECT_EQ(**v2, 99);
}
