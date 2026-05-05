#include <gtest/gtest.h>
#include "collab/document.h"
#include "collab/operation.h"

#include <thread>
#include <vector>

using namespace collab;

TEST(DocumentThreadSafety, ConcurrentReadWrite) {
    Document doc(1, "Test");
    doc.set_content("HELLO");

    constexpr int NUM_OPS = 500;
    constexpr int NUM_READERS = 4;

    std::jthread writer([&doc] {
        for (uint32_t i = 0; i < NUM_OPS; ++i) {
            doc.apply_with_ot(make_insert(0, "X", 1, i));
        }
    });

    std::vector<std::jthread> readers;
    for (int r = 0; r < NUM_READERS; ++r) {
        readers.emplace_back([&doc] {
            for (int i = 0; i < NUM_OPS; ++i) {
                auto content = doc.get_content();
                auto rev = doc.revision();
                auto hist = doc.history_size();
                (void)content;
                (void)rev;
                (void)hist;
            }
        });
    }

    writer.join();
    readers.clear();

    EXPECT_EQ(doc.revision(), static_cast<uint32_t>(NUM_OPS));
}

TEST(DocumentThreadSafety, ConcurrentSubscribeUnsubscribe) {
    Document doc(1, "Test");

    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 200;

    std::vector<std::jthread> threads;
    for (uint32_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&doc, t] {
            uint32_t base = t * OPS_PER_THREAD;
            for (uint32_t i = 0; i < OPS_PER_THREAD; ++i) {
                doc.subscribe(base + i);
            }
            for (uint32_t i = 0; i < OPS_PER_THREAD; ++i) {
                doc.unsubscribe(base + i);
            }
        });
    }

    threads.clear();
    EXPECT_EQ(doc.subscriber_count(), 0);
}

TEST(DocumentThreadSafety, MultiProducerSingleConsumer) {
    Document doc(1, "Test");
    doc.set_content("START");

    constexpr uint32_t NUM_PRODUCERS = 4;
    constexpr uint32_t OPS_PER_PRODUCER = 100;
    constexpr uint32_t TOTAL_OPS = NUM_PRODUCERS * OPS_PER_PRODUCER;

    std::vector<std::jthread> producers;
    for (uint32_t p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&doc, p] {
            uint32_t userId = p + 1;
            for (uint32_t i = 0; i < OPS_PER_PRODUCER; ++i) {
                auto op = make_insert(0, "X", userId, 0);
                doc.enqueue_operation(std::move(op));
            }
        });
    }

    producers.clear();

    size_t total_processed = 0;
    size_t batch;
    while ((batch = doc.process_queue()) > 0) {
        total_processed += batch;
    }

    EXPECT_EQ(total_processed, TOTAL_OPS);
    EXPECT_EQ(doc.revision(), TOTAL_OPS);
}
