#include <gtest/gtest.h>
#include "collab/document.h"
#include "collab/operation.h"

using namespace collab;

TEST(DocumentLifecycle, NewDocumentIsEmpty) {
    Document doc(1, "Test");
    EXPECT_EQ(doc.get_content(), "");
    EXPECT_EQ(doc.revision(), 0u);
    EXPECT_EQ(doc.subscriber_count(), 0);
    EXPECT_FALSE(doc.is_dirty());
}

TEST(DocumentLifecycle, ApplySetsDirtyFlag) {
    Document doc(1, "Test");
    doc.set_content("HELLO");
    doc.mark_saved();
    EXPECT_FALSE(doc.is_dirty());

    doc.apply_with_ot(make_insert(0, "X", 1, 0));
    EXPECT_TRUE(doc.is_dirty());
}

TEST(DocumentLifecycle, ClearDirtyAfterSave) {
    Document doc(1, "Test");
    doc.set_content("HELLO");
    doc.apply_with_ot(make_insert(0, "X", 1, 0));
    EXPECT_TRUE(doc.is_dirty());

    doc.mark_saved();
    EXPECT_FALSE(doc.is_dirty());
}

TEST(DocumentLifecycle, SubscriberCount) {
    Document doc(1, "Test");
    EXPECT_EQ(doc.subscriber_count(), 0);

    doc.subscribe(10);
    EXPECT_EQ(doc.subscriber_count(), 1);

    doc.subscribe(20);
    EXPECT_EQ(doc.subscriber_count(), 2);

    doc.subscribe(10);
    EXPECT_EQ(doc.subscriber_count(), 2);

    doc.unsubscribe(10);
    EXPECT_EQ(doc.subscriber_count(), 1);

    doc.unsubscribe(20);
    EXPECT_EQ(doc.subscriber_count(), 0);
}

TEST(DocumentLifecycle, HistoryGrowsWithOperations) {
    Document doc(1, "Test");
    doc.set_content("ABCDE");

    EXPECT_EQ(doc.history_size(), 0u);

    doc.apply_with_ot(make_insert(0, "X", 1, 0));
    EXPECT_EQ(doc.history_size(), 1u);

    doc.apply_with_ot(make_insert(0, "Y", 1, 1));
    EXPECT_EQ(doc.history_size(), 2u);

    doc.apply_with_ot(make_insert(0, "Z", 1, 2));
    EXPECT_EQ(doc.history_size(), 3u);
}

TEST(DocumentLifecycle, HistoryRingBufferOverflow) {
    Document doc(1, "Test");
    doc.set_content("ABC");
    doc.set_history_limit(3);

    for (uint32_t i = 0; i < 5; ++i) {
        doc.apply_with_ot(make_insert(0, "X", 1, i));
    }

    EXPECT_EQ(doc.history_size(), 3u);
    EXPECT_EQ(doc.revision(), 5u);
    EXPECT_EQ(doc.oldest_revision_in_history(), 2u);
}

TEST(DocumentLifecycle, LoadFromDisk) {
    Document doc(1, "Test");
    doc.load_content("Loaded content");
    EXPECT_EQ(doc.get_content(), "Loaded content");
    EXPECT_FALSE(doc.is_dirty());
}

TEST(DocumentLifecycle, SnapshotThreshold) {
    Document doc(1, "Test");
    doc.set_content("ABC");
    doc.set_snapshot_threshold(3);

    EXPECT_FALSE(doc.needs_snapshot());

    doc.apply_with_ot(make_insert(0, "X", 1, 0));
    EXPECT_FALSE(doc.needs_snapshot());

    doc.apply_with_ot(make_insert(0, "Y", 1, 1));
    EXPECT_FALSE(doc.needs_snapshot());

    doc.apply_with_ot(make_insert(0, "Z", 1, 2));
    EXPECT_TRUE(doc.needs_snapshot());

    doc.mark_snapshot_created();
    EXPECT_FALSE(doc.needs_snapshot());
}
