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

QByteArray encode_doc_list_request() {
    return to_qbytes(server::serialize(server::DocListRequestMsg{}));
}

QByteArray encode_doc_create_request(const QString& title) {
    return to_qbytes(server::serialize(
        server::DocCreateRequestMsg{.title = title.toStdString()}));
}

QByteArray encode_doc_join_request(uint32_t docId) {
    return to_qbytes(server::serialize(server::DocJoinRequestMsg{.docId = docId}));
}

QByteArray encode_doc_leave_request(uint32_t docId) {
    return to_qbytes(server::serialize(server::DocLeaveRequestMsg{.docId = docId}));
}

QByteArray encode_doc_delete_request(uint32_t docId) {
    return to_qbytes(server::serialize(server::DocDeleteRequestMsg{.docId = docId}));
}

QByteArray encode_doc_share_request(uint32_t docId,
                                    const QString& targetUsername,
                                    const QString& role) {
    return to_qbytes(server::serialize(server::DocShareRequestMsg{
        .docId = docId,
        .targetUsername = targetUsername.toStdString(),
        .role = role.toStdString(),
    }));
}

QByteArray encode_cursor_update(uint32_t docId,
                                uint32_t position,
                                std::optional<uint32_t> selectionStart,
                                std::optional<uint32_t> selectionEnd) {
    return to_qbytes(server::serialize(server::CursorUpdateMsg{
        .docId = docId,
        .position = position,
        .selectionStart = selectionStart,
        .selectionEnd = selectionEnd,
    }));
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

namespace {

template <typename T>
std::optional<T> parse_as(const QByteArray& payload) {
    try {
        auto j = nlohmann::json::parse(payload.constData(),
                                       payload.constData() + payload.size());
        return j.get<T>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

} // namespace

std::optional<server::AuthResponseMsg> parse_auth_response(const QByteArray& payload) {
    return parse_as<server::AuthResponseMsg>(payload);
}

std::optional<server::DocListResponseMsg> parse_doc_list_response(const QByteArray& payload) {
    return parse_as<server::DocListResponseMsg>(payload);
}

std::optional<server::DocCreateResponseMsg> parse_doc_create_response(const QByteArray& payload) {
    return parse_as<server::DocCreateResponseMsg>(payload);
}

std::optional<server::DocJoinResponseMsg> parse_doc_join_response(const QByteArray& payload) {
    return parse_as<server::DocJoinResponseMsg>(payload);
}

std::optional<server::DocDeleteResponseMsg> parse_doc_delete_response(const QByteArray& payload) {
    return parse_as<server::DocDeleteResponseMsg>(payload);
}

std::optional<server::CursorBroadcastMsg> parse_cursor_broadcast(const QByteArray& payload) {
    return parse_as<server::CursorBroadcastMsg>(payload);
}

std::optional<server::UserJoinedMsg> parse_user_joined(const QByteArray& payload) {
    return parse_as<server::UserJoinedMsg>(payload);
}

std::optional<server::UserLeftMsg> parse_user_left(const QByteArray& payload) {
    return parse_as<server::UserLeftMsg>(payload);
}

std::optional<server::ErrorMsg> parse_error(const QByteArray& payload) {
    return parse_as<server::ErrorMsg>(payload);
}

} // namespace collab_client
