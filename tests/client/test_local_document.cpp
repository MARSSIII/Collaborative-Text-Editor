#include "client/local_document.h"

#include "collab/operation.h"

#include <gtest/gtest.h>

#include <atomic>
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
    LocalDocument doc(std::string(1000, 'A'), 0);

    std::atomic<bool> stop{false};
    std::atomic<size_t> reads{0};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto snap = doc.snapshot();
                ASSERT_FALSE(snap.empty());
                const char first = snap[0];
                for (char c : snap) ASSERT_EQ(c, first);
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int i = 0; i < 200; ++i) {
        const char letter = (i % 2) ? 'B' : 'A';
        doc.reset(std::string(1000, letter), static_cast<uint32_t>(i));
    }

    stop.store(true);
    for (auto& t : readers) t.join();
    EXPECT_GT(reads.load(), 0u);
}
