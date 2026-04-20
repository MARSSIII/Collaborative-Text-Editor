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

TEST(RemoteCursorsModel, IgnoresLocalUser) {
    RemoteCursorsModel m(42);
    m.applyBroadcast(make_broadcast(1, {
        {42, "me", "#ff0000", 5, std::nullopt, std::nullopt},
        {7,  "bob", "#00ff00", 9, std::nullopt, std::nullopt},
    }));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].userId, 7u);
}

TEST(RemoteCursorsModel, BroadcastReplacesState) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "#00ff00", 3, std::nullopt, std::nullopt},
        {3, "carol", "#0000ff", 8, std::nullopt, std::nullopt},
    }));
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "#00ff00", 11, std::nullopt, std::nullopt},
    }));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].userId, 2u);
    EXPECT_EQ(out[0].position, 11u);
}

TEST(RemoteCursorsModel, InsertBeforeShiftsCursorRight) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "#00ff00", 10, std::nullopt, std::nullopt},
    }));
    auto op = collab::make_insert(5, "XXX",  2,  0);
    m.applyRemoteOperation(op);
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].position, 13u);
}

TEST(RemoteCursorsModel, DeleteBeforeShiftsCursorLeft) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "#00ff00", 10, std::nullopt, std::nullopt},
    }));
    auto op = collab::make_delete(2, 3, "abc", 2, 0);
    m.applyRemoteOperation(op);
    auto out = m.cursors();
    EXPECT_EQ(out[0].position, 7u);
}

TEST(RemoteCursorsModel, DeleteSpanningCursorCollapsesToStart) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "#00ff00", 7, std::nullopt, std::nullopt},
    }));
    auto op = collab::make_delete(5, 5, "xxxxx", 2, 0);
    m.applyRemoteOperation(op);
    auto out = m.cursors();
    EXPECT_EQ(out[0].position, 5u);
}

TEST(RemoteCursorsModel, TranslatesSelection) {
    RemoteCursorsModel m(1);
    server::CursorInfo ci{2, "bob", "#00ff00", 10, 6u, 12u};
    m.applyBroadcast(make_broadcast(1, {ci}));
    auto op = collab::make_insert(3, "aa", 2, 0);
    m.applyRemoteOperation(op);
    auto out = m.cursors();
    EXPECT_EQ(out[0].position, 12u);
    ASSERT_TRUE(out[0].selectionStart.has_value());
    EXPECT_EQ(*out[0].selectionStart, 8u);
    EXPECT_EQ(*out[0].selectionEnd, 14u);
}

TEST(RemoteCursorsModel, RemoveUserRemoves) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "#00ff00", 3, std::nullopt, std::nullopt},
        {3, "carol", "#0000ff", 5, std::nullopt, std::nullopt},
    }));
    m.removeUser(2);
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].userId, 3u);
}

TEST(RemoteCursorsModel, InvalidColorFallsBackToGrey) {
    RemoteCursorsModel m(1);
    m.applyBroadcast(make_broadcast(1, {
        {2, "bob", "not-a-color", 3, std::nullopt, std::nullopt},
    }));
    auto out = m.cursors();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_TRUE(out[0].color.isValid());
}
