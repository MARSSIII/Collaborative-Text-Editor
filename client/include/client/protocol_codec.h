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

struct ParsedEnvelope {
    server::MessageType type;
    QByteArray payload;  // the original JSON payload, for further type-specific parsing
};

// Extracts just the message "type" field. Returns Unknown for malformed payloads.
ParsedEnvelope parse_envelope(const QByteArray& payload);

std::optional<server::AuthResponseMsg> parse_auth_response(const QByteArray& payload);

} // namespace collab_client
