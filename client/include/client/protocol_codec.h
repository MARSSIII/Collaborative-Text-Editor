#pragma once

#include "collab_protocol/protocol.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace collab_client {

QByteArray encode_auth_request(const QString& action,
                               const QString& username,
                               const QString& password);
QByteArray encode_doc_list_request();
QByteArray encode_doc_create_request(const QString& title);
QByteArray encode_doc_join_request(uint32_t docId);
QByteArray encode_doc_leave_request(uint32_t docId);
QByteArray encode_doc_delete_request(uint32_t docId);
QByteArray encode_doc_share_request(uint32_t docId, const QString& targetUsername, const QString& role);
QByteArray encode_cursor_update(uint32_t docId,
                                uint32_t position,
                                std::optional<uint32_t> selectionStart,
                                std::optional<uint32_t> selectionEnd);

struct ParsedEnvelope {
    server::MessageType type;
    QByteArray payload;
};

ParsedEnvelope parse_envelope(const QByteArray& payload);

std::optional<server::AuthResponseMsg> parse_auth_response(const QByteArray& payload);
std::optional<server::DocListResponseMsg> parse_doc_list_response(const QByteArray& payload);
std::optional<server::DocCreateResponseMsg> parse_doc_create_response(const QByteArray& payload);
std::optional<server::DocJoinResponseMsg> parse_doc_join_response(const QByteArray& payload);
std::optional<server::DocDeleteResponseMsg> parse_doc_delete_response(const QByteArray& payload);
std::optional<server::CursorBroadcastMsg> parse_cursor_broadcast(const QByteArray& payload);
std::optional<server::UserJoinedMsg> parse_user_joined(const QByteArray& payload);
std::optional<server::UserLeftMsg> parse_user_left(const QByteArray& payload);
std::optional<server::ErrorMsg> parse_error(const QByteArray& payload);

} // namespace collab_client
