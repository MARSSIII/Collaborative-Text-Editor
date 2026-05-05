#include "client/remote_cursors_model.h"
#include "collab/operation.h"
#include "collab_protocol/protocol.h"

#include <gtest/gtest.h>

using namespace collab_client;

namespace {

server::CursorBroadcastMsg make_broadcast(uint32_t docId,
                                          std::vector<server::CursorInfo> cursors) {
    server::CursorBroadcastMsg m;
    m.docId = docId;
    m.cursors = std::move(cursors);
    return m;
}

}

namespace {

server::CursorInfo bob_at(uint32_t pos,
                          std::optional<uint32_t> sel_start = std::nullopt,
                          std::optional<uint32_t> sel_end   = std::nullopt) {
    return {.userId = 2, .username = "bob", .color = "#00ff00",
            .position = pos, .selectionStart = sel_start, .selectionEnd = sel_end};
}

server::CursorInfo carol_at(uint32_t pos) {
    return {.userId = 3, .username = "carol", .color = "#0000ff",
            .position = pos, .selectionStart = std::nullopt, .selectionEnd = std::nullopt};
}

}

TEST(RemoteCursorsModel, IgnoresLocalUser) {
    RemoteCursorsModel m(42);
    m.applyBroadcast(make_broadcast(1, {
        {.userId = 42, .username = "me", .color = "#ff0000",
         .position = 5, .selectionStart = std::nullopt, .selectionEnd = std::nullopt},
        {.userId = 7, .username = "bob", .color = "#00ff00",
         .position = 9, .selectionStart = std::nullopt, .selectionEnd = std::nullopt},
    }));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].userId, 7u);
}

TEST(RemoteCursorsModel, BroadcastReplacesState) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {bob_at(3), carol_at(8)}));
    m.applyBroadcast(make_broadcast(1, {bob_at(11)}));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].userId, 2u);
    EXPECT_EQ(out[0].position, 11u);
}

TEST(RemoteCursorsModel, InsertBeforeShiftsCursorRight) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {bob_at(10)}));
    m.applyRemoteOperation(collab::make_insert(5, "XXX", 2, 0));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].position, 13u);
}

TEST(RemoteCursorsModel, DeleteBeforeShiftsCursorLeft) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {bob_at(10)}));
    m.applyRemoteOperation(collab::make_delete(2, 3, "abc", 2, 0));
    auto out = m.cursors();
    EXPECT_EQ(out[0].position, 7u);
}

TEST(RemoteCursorsModel, DeleteSpanningCursorCollapsesToStart) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {bob_at(7)}));
    m.applyRemoteOperation(collab::make_delete(5, 5, "xxxxx", 2, 0));
    auto out = m.cursors();
    EXPECT_EQ(out[0].position, 5u);
}

TEST(RemoteCursorsModel, TranslatesSelection) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {bob_at(10, 6u, 12u)}));
    m.applyRemoteOperation(collab::make_insert(3, "aa", 2, 0));
    auto out = m.cursors();
    EXPECT_EQ(out[0].position, 12u);
    ASSERT_TRUE(out[0].selectionStart.has_value());
    EXPECT_EQ(*out[0].selectionStart, 8u);
    EXPECT_EQ(*out[0].selectionEnd, 14u);
}

TEST(RemoteCursorsModel, RemoveUserRemoves) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {bob_at(3), carol_at(5)}));
    m.removeUser(2);
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].userId, 3u);
}

TEST(RemoteCursorsModel, InvalidColorFallsBackToGrey) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {.userId = 2, .username = "bob", .color = "not-a-color",
         .position = 3, .selectionStart = std::nullopt, .selectionEnd = std::nullopt},
    }));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_TRUE(out[0].color.isValid());
}
