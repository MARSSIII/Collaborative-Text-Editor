#pragma once

#include "collab_protocol/protocol.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace collab_client {

// Produces the raw JSON payload (no length prefix) for outgoing messages.
// NetworkManager wraps it into a length-prefixed frame via server::encode_frame.
QByteArray encode_auth_request(const QString& action,
                               const QString& username,
                               const QString& password);
QByteArray encode_doc_list_request();
QByteArray encode_doc_create_request(const QString& title);
QByteArray encode_doc_join_request(uint32_t docId);
QByteArray encode_doc_leave_request(uint32_t docId);
QByteArray encode_doc_delete_request(uint32_t docId);

struct ParsedEnvelope {
    server::MessageType type;
    QByteArray payload;  // the original JSON payload, for further type-specific parsing
};

// Extracts just the message "type" field. Returns Unknown for malformed payloads.
ParsedEnvelope parse_envelope(const QByteArray& payload);

// Typed parsers. Each returns nullopt on malformed input. The typical client
// flow is: parse_envelope() to get the type, then call the matching parse_X().
std::optional<server::AuthResponseMsg> parse_auth_response(const QByteArray& payload);
std::optional<server::DocListResponseMsg> parse_doc_list_response(const QByteArray& payload);
std::optional<server::DocCreateResponseMsg> parse_doc_create_response(const QByteArray& payload);
std::optional<server::DocJoinResponseMsg> parse_doc_join_response(const QByteArray& payload);
std::optional<server::DocDeleteResponseMsg> parse_doc_delete_response(const QByteArray& payload);
std::optional<server::ErrorMsg> parse_error(const QByteArray& payload);

} // namespace collab_client
