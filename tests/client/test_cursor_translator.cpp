#include "client/cursor_translator.h"

#include "collab/operation.h"

#include <gtest/gtest.h>

using collab_client::cursor_translator::translate;
using collab::make_insert;
using collab::make_delete;

TEST(CursorTranslator, InsertBeforeMyCursorShiftsRight) {
    EXPECT_EQ(translate(10, make_insert(3, "abc", 1, 0)), 13u);
}

TEST(CursorTranslator, InsertAtMyCursorShiftsRight) {
    EXPECT_EQ(translate(10, make_insert(10, "X", 1, 0)), 11u);
}

TEST(CursorTranslator, InsertAfterMyCursorDoesNotShift) {
    EXPECT_EQ(translate(10, make_insert(15, "abc", 1, 0)), 10u);
}

TEST(CursorTranslator, DeleteEntirelyBeforeMyCursorShiftsLeft) {
    EXPECT_EQ(translate(10, make_delete(2, 3, "", 1, 0)), 7u);
}

TEST(CursorTranslator, DeleteAdjacentBeforeMyCursorShiftsLeft) {
    EXPECT_EQ(translate(10, make_delete(5, 5, "", 1, 0)), 5u);
}

TEST(CursorTranslator, DeleteAtMyCursorDoesNotShift) {
    EXPECT_EQ(translate(10, make_delete(10, 3, "", 1, 0)), 10u);
}

TEST(CursorTranslator, DeleteAfterMyCursorDoesNotShift) {
    EXPECT_EQ(translate(10, make_delete(15, 3, "", 1, 0)), 10u);
}

TEST(CursorTranslator, DeleteSpanningMyCursorCollapsesToStart) {
    EXPECT_EQ(translate(10, make_delete(8, 5, "", 1, 0)), 8u);
}

TEST(CursorTranslator, DeleteContainsMyCursorAtEndCollapsesToStart) {
    EXPECT_EQ(translate(10, make_delete(5, 10, "", 1, 0)), 5u);
}

TEST(CursorTranslator, NoopInsertDoesNotShift) {
    EXPECT_EQ(translate(10, make_insert(5, "", 1, 0)), 10u);
}

TEST(CursorTranslator, NoopDeleteDoesNotShift) {
    EXPECT_EQ(translate(10, make_delete(5, 0, "", 1, 0)), 10u);
}

TEST(CursorTranslator, MyCursorAtZeroInsertAtZero) {
    EXPECT_EQ(translate(0, make_insert(0, "X", 1, 0)), 1u);
}

TEST(CursorTranslator, CyrillicUtf8LengthInInsert) {
    const std::string cyrillic = "Привет";
    EXPECT_EQ(translate(5, make_insert(2, cyrillic, 1, 0)),
              5u + static_cast<uint32_t>(cyrillic.size()));
}
