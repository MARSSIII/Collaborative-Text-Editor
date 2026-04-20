#include "client/protocol_codec.h"

#include <nlohmann/json.hpp>

namespace collab_client {

namespace {

QByteArray to_qbytes(const std::string& s) {
    return QByteArray(s.data(), static_cast<int>(s.size()));
}

} // namespace

QByteArray encode_auth_request(const QString& action,
                               const QString& username,
                               const QString& password) {
    server::AuthRequestMsg msg{
        .action = action.toStdString(),
        .username = username.toStdString(),
        .password = password.toStdString(),
    };
    return to_qbytes(server::serialize(msg));
}

ParsedEnvelope parse_envelope(const QByteArray& payload) {
    try {
        auto j = nlohmann::json::parse(payload.constData(),
                                       payload.constData() + payload.size());
        if (!j.contains("type") || !j["type"].is_string()) {
            return {server::MessageType::Unknown, payload};
        }
        return {server::parse_message_type(j["type"].get<std::string>()), payload};
    } catch (const nlohmann::json::exception&) {
        return {server::MessageType::Unknown, payload};
    }
}

std::optional<server::AuthResponseMsg> parse_auth_response(const QByteArray& payload) {
    try {
        auto j = nlohmann::json::parse(payload.constData(),
                                       payload.constData() + payload.size());
        return j.get<server::AuthResponseMsg>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

} // namespace collab_client
