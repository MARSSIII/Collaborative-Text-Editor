#include "client/local_document.h"

#include "collab/operation.h"

#include <gtest/gtest.h>

#include <atomic>
#include <latch>
#include <thread>
#include <vector>

using namespace collab_client;
using namespace collab;

TEST(LocalDocument, InitialSnapshot) {
    LocalDocument doc("Hello", 5);
    EXPECT_EQ(doc.snapshot(), "Hello");
    EXPECT_EQ(doc.revision(), 5u);
}

TEST(LocalDocument, ApplyInsert) {
    LocalDocument doc("Hello", 0);
    doc.apply(make_insert(5, " world", 1, 0));
    EXPECT_EQ(doc.snapshot(), "Hello world");
}

TEST(LocalDocument, ApplyDelete) {
    LocalDocument doc("Hello world", 0);
    doc.apply(make_delete(5, 6, " world", 1, 0));
    EXPECT_EQ(doc.snapshot(), "Hello");
}

TEST(LocalDocument, Reset) {
    LocalDocument doc("old", 1);
    doc.reset("new content", 42);
    EXPECT_EQ(doc.snapshot(), "new content");
    EXPECT_EQ(doc.revision(), 42u);
}

TEST(LocalDocument, ConcurrentReadersDuringWriter) {
    constexpr int NUM_READERS = 4;
    constexpr int NUM_WRITES = 200;
    constexpr size_t DOC_SIZE = 1000;

    LocalDocument doc(std::string(DOC_SIZE, 'A'), 0);

    std::atomic<bool> stop{false};
    std::atomic<size_t> reads{0};
    std::latch readers_started{NUM_READERS};

    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&] {
            readers_started.count_down();
            while (!stop.load(std::memory_order_relaxed)) {
                auto snap = doc.snapshot();
                ASSERT_FALSE(snap.empty());
                const char first = snap[0];
                for (char c : snap) ASSERT_EQ(c, first);
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    readers_started.wait();

    for (int i = 0; i < NUM_WRITES; ++i) {
        const char letter = (i % 2) ? 'B' : 'A';
        doc.reset(std::string(DOC_SIZE, letter), static_cast<uint32_t>(i));
    }

    stop.store(true);
    for (auto& t : readers) t.join();
    EXPECT_GE(reads.load(), static_cast<size_t>(NUM_READERS));
}
