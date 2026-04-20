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
