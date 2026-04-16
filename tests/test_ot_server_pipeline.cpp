#include <gtest/gtest.h>
#include "collab/document.h"
#include "collab/operation.h"

using namespace collab;

class ServerOTTest : public ::testing::Test {
protected:
    Document doc{1, "Test"};
    void SetUp() override {
        doc.set_content("ABCDE");
    }
};

TEST_F(ServerOTTest, SingleOperation) {
    auto result = doc.apply_with_ot(make_insert(1, "X", 1, 0));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(doc.get_content(), "AXBCDE");
    EXPECT_EQ(doc.revision(), 1u);
}

TEST_F(ServerOTTest, SequentialSameClient) {
    auto r1 = doc.apply_with_ot(make_insert(1, "X", 1, 0));
    EXPECT_TRUE(r1.success);

    auto r2 = doc.apply_with_ot(make_insert(3, "Y", 1, 1));
    EXPECT_TRUE(r2.success);
    EXPECT_EQ(doc.get_content(), "AXBYCDE");
    EXPECT_EQ(doc.revision(), 2u);
}

TEST_F(ServerOTTest, ConcurrentTwoClients) {
    auto r1 = doc.apply_with_ot(make_insert(1, "X", 1, 0));
    EXPECT_TRUE(r1.success);

    auto r2 = doc.apply_with_ot(make_delete(3, 1, "D", 2, 0));
    EXPECT_TRUE(r2.success);
    EXPECT_EQ(doc.get_content(), "AXBCE");
    EXPECT_EQ(doc.revision(), 2u);
}

TEST_F(ServerOTTest, ConcurrentThreeClients) {
    auto r1 = doc.apply_with_ot(make_insert(0, "X", 1, 0));
    EXPECT_TRUE(r1.success);

    auto r2 = doc.apply_with_ot(make_insert(5, "Y", 2, 0));
    EXPECT_TRUE(r2.success);
    EXPECT_EQ(doc.get_content(), "XABCDEY");

    auto r3 = doc.apply_with_ot(make_delete(2, 1, "C", 3, 0));
    EXPECT_TRUE(r3.success);
    EXPECT_EQ(doc.get_content(), "XABDEY");
    EXPECT_EQ(doc.revision(), 3u);
}

TEST_F(ServerOTTest, StaleRevision) {
    doc.apply_with_ot(make_insert(0, "1", 1, 0));
    doc.apply_with_ot(make_insert(0, "2", 1, 1));
    doc.apply_with_ot(make_insert(0, "3", 1, 2));
    doc.apply_with_ot(make_insert(0, "4", 1, 3));
    doc.apply_with_ot(make_insert(0, "5", 1, 4));

    auto result = doc.apply_with_ot(make_insert(0, "Z", 2, 0));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(doc.get_content(), "54321ZABCDE");
}

TEST_F(ServerOTTest, RevisionTooOld) {
    doc.set_history_limit(3);

    doc.apply_with_ot(make_insert(0, "1", 1, 0));
    doc.apply_with_ot(make_insert(0, "2", 1, 1));
    doc.apply_with_ot(make_insert(0, "3", 1, 2));
    doc.apply_with_ot(make_insert(0, "4", 1, 3));
    doc.apply_with_ot(make_insert(0, "5", 1, 4));

    auto result = doc.apply_with_ot(make_insert(0, "Z", 2, 0));
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "revision_too_old");
}
