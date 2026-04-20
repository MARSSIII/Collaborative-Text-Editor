#include "client/protocol_codec.h"
#include "collab_protocol/protocol.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <QString>

using namespace collab_client;

namespace {

QByteArray to_frame_payload(const std::string& s) {
    return QByteArray(s.data(), static_cast<int>(s.size()));
}

} // namespace

TEST(ProtocolCodec, AuthRequestRoundTripAscii) {
    auto payload = encode_auth_request("login", "alice", "s3cureP@ss");

    auto j = nlohmann::json::parse(payload.constData(),
                                   payload.constData() + payload.size());
    EXPECT_EQ(j["type"], "auth_request");
    EXPECT_EQ(j["action"], "login");
    EXPECT_EQ(j["username"], "alice");
    EXPECT_EQ(j["password"], "s3cureP@ss");
}

TEST(ProtocolCodec, AuthRequestRoundTripCyrillic) {
    auto payload = encode_auth_request("register",
                                       QString::fromUtf8("алиса"),
                                       QString::fromUtf8("пароль123"));

    auto j = nlohmann::json::parse(payload.constData(),
                                   payload.constData() + payload.size());
    EXPECT_EQ(j["username"].get<std::string>(), "алиса");
    EXPECT_EQ(j["password"].get<std::string>(), "пароль123");
}

TEST(ProtocolCodec, ParseEnvelopeRecognisesKnownTypes) {
    auto env = parse_envelope(QByteArrayLiteral(R"({"type":"auth_response","success":true,"userId":7})"));
    EXPECT_EQ(env.type, server::MessageType::AuthResponse);
}

TEST(ProtocolCodec, ParseEnvelopeUnknownType) {
    auto env = parse_envelope(QByteArrayLiteral(R"({"type":"not_a_real_type"})"));
    EXPECT_EQ(env.type, server::MessageType::Unknown);
}

TEST(ProtocolCodec, ParseEnvelopeMalformedJson) {
    auto env = parse_envelope(QByteArrayLiteral("{not json"));
    EXPECT_EQ(env.type, server::MessageType::Unknown);
}

TEST(ProtocolCodec, ParseAuthResponseSuccess) {
    server::AuthResponseMsg source{.success = true, .userId = 42, .error = ""};
    auto payload = to_frame_payload(server::serialize(source));

    auto parsed = parse_auth_response(payload);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->success);
    EXPECT_EQ(parsed->userId, 42u);
}

TEST(ProtocolCodec, ParseAuthResponseError) {
    server::AuthResponseMsg source{.success = false,
                                   .userId = 0,
                                   .error = "invalid_credentials"};
    auto payload = to_frame_payload(server::serialize(source));

    auto parsed = parse_auth_response(payload);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->success);
    EXPECT_EQ(parsed->error, "invalid_credentials");
}

TEST(ProtocolCodec, CursorUpdateEncodesPositionOnly) {
    auto payload = encode_cursor_update(5, 42, std::nullopt, std::nullopt);
    auto j = nlohmann::json::parse(payload.constData(),
                                   payload.constData() + payload.size());
    EXPECT_EQ(j["type"], "cursor_update");
    EXPECT_EQ(j["docId"], 5u);
    EXPECT_EQ(j["position"], 42u);
    EXPECT_TRUE(!j.contains("selectionStart") || j["selectionStart"].is_null());
}

TEST(ProtocolCodec, CursorUpdateEncodesSelection) {
    auto payload = encode_cursor_update(5, 10, 4, 16);
    auto j = nlohmann::json::parse(payload.constData(),
                                   payload.constData() + payload.size());
    EXPECT_EQ(j["selectionStart"], 4u);
    EXPECT_EQ(j["selectionEnd"], 16u);
}

TEST(ProtocolCodec, ParseCursorBroadcast) {
    server::CursorBroadcastMsg src;
    src.docId = 7;
    src.cursors.push_back({1, "alice", "#ff0000", 3, std::nullopt, std::nullopt});
    src.cursors.push_back({2, "bob", "#00ff00", 9, 5u, 9u});
    auto payload = to_frame_payload(server::serialize(src));

    auto parsed = parse_cursor_broadcast(payload);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->docId, 7u);
    ASSERT_EQ(parsed->cursors.size(), 2u);
    EXPECT_EQ(parsed->cursors[0].username, "alice");
    EXPECT_EQ(parsed->cursors[0].position, 3u);
    ASSERT_TRUE(parsed->cursors[1].selectionStart.has_value());
    EXPECT_EQ(*parsed->cursors[1].selectionStart, 5u);
}

TEST(ProtocolCodec, ParseUserJoinedAndLeft) {
    server::UserJoinedMsg joined{.docId = 3, .userId = 9, .username = "carol", .color = "#1234ab"};
    auto p = to_frame_payload(server::serialize(joined));
    auto parsed = parse_user_joined(p);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->userId, 9u);
    EXPECT_EQ(parsed->color, "#1234ab");

    server::UserLeftMsg left{.docId = 3, .userId = 9, .username = "carol"};
    auto p2 = to_frame_payload(server::serialize(left));
    auto parsed2 = parse_user_left(p2);
    ASSERT_TRUE(parsed2.has_value());
    EXPECT_EQ(parsed2->userId, 9u);
}

TEST(ProtocolCodec, ParseServerShutdown) {
    server::ServerShutdownMsg src{.message = "Maintenance at 03:00"};
    auto p = to_frame_payload(server::serialize(src));
    auto parsed = parse_server_shutdown(p);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->message, "Maintenance at 03:00");
}

TEST(ProtocolCodec, ParseDocDeleted) {
    server::DocDeletedMsg src{.docId = 42};
    auto p = to_frame_payload(server::serialize(src));
    auto parsed = parse_doc_deleted(p);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->docId, 42u);
}

TEST(ProtocolCodec, ParseRoleChanged) {
    server::RoleChangedMsg src{.docId = 5, .newRole = "viewer"};
    auto p = to_frame_payload(server::serialize(src));
    auto parsed = parse_role_changed(p);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->docId, 5u);
    EXPECT_EQ(parsed->newRole, "viewer");
}
