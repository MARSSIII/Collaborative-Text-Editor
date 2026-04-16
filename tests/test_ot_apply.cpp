#include <gtest/gtest.h>
#include "collab/operation.h"
#include "collab/ot.h"

using namespace collab;

TEST(ApplyOp, InsertAtBeginning) {
    std::string doc = "HELLO";
    collab::apply(doc, make_insert(0, "X", 1, 0));
    EXPECT_EQ(doc, "XHELLO");
}

TEST(ApplyOp, InsertInMiddle) {
    std::string doc = "HELLO";
    collab::apply(doc, make_insert(2, "XX", 1, 0));
    EXPECT_EQ(doc, "HEXXLLO");
}

TEST(ApplyOp, InsertAtEnd) {
    std::string doc = "HELLO";
    collab::apply(doc, make_insert(5, "!", 1, 0));
    EXPECT_EQ(doc, "HELLO!");
}

TEST(ApplyOp, InsertIntoEmptyDoc) {
    std::string doc;
    collab::apply(doc, make_insert(0, "HELLO", 1, 0));
    EXPECT_EQ(doc, "HELLO");
}

TEST(ApplyOp, InsertEmptyString) {
    std::string doc = "HELLO";
    collab::apply(doc, make_insert(2, "", 1, 0));
    EXPECT_EQ(doc, "HELLO");
}

TEST(ApplyOp, InsertPositionClamped) {
    std::string doc = "HI";
    collab::apply(doc, make_insert(100, "!", 1, 0));
    EXPECT_EQ(doc, "HI!");
}

TEST(ApplyOp, DeleteAtBeginning) {
    std::string doc = "HELLO";
    collab::apply(doc, make_delete(0, 2, "HE", 1, 0));
    EXPECT_EQ(doc, "LLO");
}

TEST(ApplyOp, DeleteInMiddle) {
    std::string doc = "HELLO";
    collab::apply(doc, make_delete(1, 3, "ELL", 1, 0));
    EXPECT_EQ(doc, "HO");
}

TEST(ApplyOp, DeleteAtEnd) {
    std::string doc = "HELLO";
    collab::apply(doc, make_delete(3, 2, "LO", 1, 0));
    EXPECT_EQ(doc, "HEL");
}

TEST(ApplyOp, DeleteEntireDocument) {
    std::string doc = "HELLO";
    collab::apply(doc, make_delete(0, 5, "HELLO", 1, 0));
    EXPECT_EQ(doc, "");
}

TEST(ApplyOp, DeleteZeroLength) {
    std::string doc = "HELLO";
    collab::apply(doc, make_delete(2, 0, "", 1, 0));
    EXPECT_EQ(doc, "HELLO");
}

TEST(ApplyOp, DeleteBeyondEnd) {
    std::string doc = "HI";
    collab::apply(doc, make_delete(1, 100, "", 1, 0));
    EXPECT_EQ(doc, "H");
}

TEST(ApplyOp, DeleteFromEmptyDoc) {
    std::string doc;
    collab::apply(doc, make_delete(0, 5, "", 1, 0));
    EXPECT_EQ(doc, "");
}

TEST(ApplyOp, DeletePositionBeyondEnd) {
    std::string doc = "AB";
    collab::apply(doc, make_delete(50, 3, "", 1, 0));
    EXPECT_EQ(doc, "AB");
}

TEST(ApplyOp, InsertIntoEmptyDocAtNonZeroPos) {
    std::string doc;
    collab::apply(doc, make_insert(999, "X", 1, 0));
    EXPECT_EQ(doc, "X");
}

TEST(ApplyOp, NoopInsertIsIgnored) {
    std::string doc = "HELLO";
    collab::apply(doc, make_insert(2, "", 1, 0));
    EXPECT_EQ(doc, "HELLO");
}

TEST(ApplyOp, NoopDeleteIsIgnored) {
    std::string doc = "HELLO";
    collab::apply(doc, make_delete(2, 0, "", 1, 0));
    EXPECT_EQ(doc, "HELLO");
}
