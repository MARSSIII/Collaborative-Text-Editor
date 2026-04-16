#include "server/protocol.h"

#include <stdexcept>
#include <unordered_map>

namespace server {

// ============================================================
// MessageType ↔ string
// ============================================================

static const std::unordered_map<std::string, MessageType> kStringToType = {
    {"auth_request",              MessageType::AuthRequest},
    {"auth_response",             MessageType::AuthResponse},
    {"doc_list_request",          MessageType::DocListRequest},
    {"doc_list_response",         MessageType::DocListResponse},
    {"doc_create_request",        MessageType::DocCreateRequest},
    {"doc_create_response",       MessageType::DocCreateResponse},
    {"doc_join_request",          MessageType::DocJoinRequest},
    {"doc_join_response",         MessageType::DocJoinResponse},
    {"doc_leave_request",         MessageType::DocLeaveRequest},
    {"doc_leave_response",        MessageType::DocLeaveResponse},
    {"doc_delete_request",        MessageType::DocDeleteRequest},
    {"doc_delete_response",       MessageType::DocDeleteResponse},
    {"doc_share_request",         MessageType::DocShareRequest},
    {"doc_share_response",        MessageType::DocShareResponse},
    {"operation",                 MessageType::Operation},
    {"operation_ack",             MessageType::OperationAck},
    {"operation_broadcast",       MessageType::OperationBroadcast},
    {"cursor_update",             MessageType::CursorUpdate},
    {"cursor_broadcast",          MessageType::CursorBroadcast},
    {"user_joined",               MessageType::UserJoined},
    {"user_left",                 MessageType::UserLeft},
    {"role_changed",              MessageType::RoleChanged},
    {"doc_deleted",               MessageType::DocDeleted},
    {"server_shutdown",           MessageType::ServerShutdown},
    {"error",                     MessageType::Error},
    {"undo_request",              MessageType::UndoRequest},
    {"redo_request",              MessageType::RedoRequest},
    {"snapshot_list_request",     MessageType::SnapshotListRequest},
    {"snapshot_list_response",    MessageType::SnapshotListResponse},
    {"snapshot_view_request",     MessageType::SnapshotViewRequest},
    {"snapshot_view_response",    MessageType::SnapshotViewResponse},
    {"snapshot_restore_request",  MessageType::SnapshotRestoreRequest},
    {"snapshot_restore_response", MessageType::SnapshotRestoreResponse},
    {"rate_limit",                MessageType::RateLimit},
};

static const std::unordered_map<MessageType, std::string> kTypeToString = [] {
    std::unordered_map<MessageType, std::string> m;
    for (auto& [s, t] : kStringToType) m[t] = s;
    return m;
}();

MessageType parse_message_type(const std::string& type_str) {
    auto it = kStringToType.find(type_str);
    return it != kStringToType.end() ? it->second : MessageType::Unknown;
}

std::string message_type_to_string(MessageType type) {
    auto it = kTypeToString.find(type);
    return it != kTypeToString.end() ? it->second : "unknown";
}

// ============================================================
// from_json (C→S deserialization)
// ============================================================

void from_json(const nlohmann::json& j, AuthRequestMsg& m) {
    j.at("action").get_to(m.action);
    j.at("username").get_to(m.username);
    j.at("password").get_to(m.password);
}

void from_json(const nlohmann::json& j, DocCreateRequestMsg& m) {
    j.at("title").get_to(m.title);
}

void from_json(const nlohmann::json& j, DocJoinRequestMsg& m) {
    j.at("docId").get_to(m.docId);
}

void from_json(const nlohmann::json& j, DocLeaveRequestMsg& m) {
    j.at("docId").get_to(m.docId);
}

void from_json(const nlohmann::json& j, DocDeleteRequestMsg& m) {
    j.at("docId").get_to(m.docId);
}

void from_json(const nlohmann::json& j, DocShareRequestMsg& m) {
    j.at("docId").get_to(m.docId);
    j.at("targetUsername").get_to(m.targetUsername);
    j.at("role").get_to(m.role);
}

void from_json(const nlohmann::json& j, OpEntry& m) {
    j.at("op").get_to(m.op);
    j.at("pos").get_to(m.pos);
    if (m.op == "insert") {
        j.at("text").get_to(m.text);
    } else if (m.op == "delete") {
        j.at("len").get_to(m.len);
    }
}

void from_json(const nlohmann::json& j, OperationMsg& m) {
    j.at("docId").get_to(m.docId);
    j.at("revision").get_to(m.revision);
    j.at("ops").get_to(m.ops);
}

void from_json(const nlohmann::json& j, CursorUpdateMsg& m) {
    j.at("docId").get_to(m.docId);
    j.at("position").get_to(m.position);
    if (j.contains("selectionStart") && !j["selectionStart"].is_null()) {
        m.selectionStart = j["selectionStart"].get<uint32_t>();
    }
    if (j.contains("selectionEnd") && !j["selectionEnd"].is_null()) {
        m.selectionEnd = j["selectionEnd"].get<uint32_t>();
    }
}

// ============================================================
// to_json (S→C serialization)
// ============================================================

void to_json(nlohmann::json& j, const AuthResponseMsg& m) {
    j = {{"type", "auth_response"}, {"success", m.success}};
    if (m.success) {
        j["userId"] = m.userId;
    } else {
        j["error"] = m.error;
    }
}

void to_json(nlohmann::json& j, const DocListEntry& m) {
    j = {{"docId", m.docId}, {"title", m.title},
         {"role", m.role}, {"onlineCount", m.onlineCount}};
}

void to_json(nlohmann::json& j, const DocListResponseMsg& m) {
    j = {{"type", "doc_list_response"}, {"documents", m.documents}};
}

void to_json(nlohmann::json& j, const DocCreateResponseMsg& m) {
    j = {{"type", "doc_create_response"}, {"success", m.success}};
    if (m.success) {
        j["docId"] = m.docId;
    }
}

void to_json(nlohmann::json& j, const UserInfo& m) {
    j = {{"userId", m.userId}, {"username", m.username}, {"color", m.color}};
}

void to_json(nlohmann::json& j, const DocJoinResponseMsg& m) {
    j = {{"type", "doc_join_response"}, {"success", m.success}};
    if (m.success) {
        j["docId"] = m.docId;
        j["title"] = m.title;
        j["content"] = m.content;
        j["revision"] = m.revision;
        j["role"] = m.role;
        j["users"] = m.users;
    } else {
        j["error"] = m.error;
    }
}

void to_json(nlohmann::json& j, const DocLeaveResponseMsg& m) {
    j = {{"type", "doc_leave_response"}, {"success", m.success}};
}

void to_json(nlohmann::json& j, const DocDeleteResponseMsg& m) {
    j = {{"type", "doc_delete_response"}, {"success", m.success}};
}

void to_json(nlohmann::json& j, const DocShareResponseMsg& m) {
    j = {{"type", "doc_share_response"}, {"success", m.success}};
}

void to_json(nlohmann::json& j, const OperationAckMsg& m) {
    j = {{"type", "operation_ack"}, {"docId", m.docId}, {"revision", m.revision}};
}

void to_json(nlohmann::json& j, const OpEntry& m) {
    j = {{"op", m.op}, {"pos", m.pos}};
    if (m.op == "insert") {
        j["text"] = m.text;
    } else if (m.op == "delete") {
        j["len"] = m.len;
    }
}

void to_json(nlohmann::json& j, const OperationBroadcastMsg& m) {
    j = {{"type", "operation_broadcast"}, {"docId", m.docId},
         {"userId", m.userId}, {"username", m.username},
         {"revision", m.revision}, {"ops", m.ops}};
}

void to_json(nlohmann::json& j, const CursorInfo& m) {
    j = {{"userId", m.userId}, {"username", m.username},
         {"color", m.color}, {"position", m.position}};
    j["selectionStart"] = m.selectionStart ? nlohmann::json(*m.selectionStart) : nlohmann::json(nullptr);
    j["selectionEnd"] = m.selectionEnd ? nlohmann::json(*m.selectionEnd) : nlohmann::json(nullptr);
}

void to_json(nlohmann::json& j, const CursorBroadcastMsg& m) {
    j = {{"type", "cursor_broadcast"}, {"docId", m.docId}, {"cursors", m.cursors}};
}

void to_json(nlohmann::json& j, const UserJoinedMsg& m) {
    j = {{"type", "user_joined"}, {"docId", m.docId},
         {"userId", m.userId}, {"username", m.username}, {"color", m.color}};
}

void to_json(nlohmann::json& j, const UserLeftMsg& m) {
    j = {{"type", "user_left"}, {"docId", m.docId},
         {"userId", m.userId}, {"username", m.username}};
}

void to_json(nlohmann::json& j, const RoleChangedMsg& m) {
    j = {{"type", "role_changed"}, {"docId", m.docId}, {"newRole", m.newRole}};
}

void to_json(nlohmann::json& j, const DocDeletedMsg& m) {
    j = {{"type", "doc_deleted"}, {"docId", m.docId}};
}

void to_json(nlohmann::json& j, const ServerShutdownMsg& m) {
    j = {{"type", "server_shutdown"}, {"message", m.message}};
}

void to_json(nlohmann::json& j, const ErrorMsg& m) {
    j = {{"type", "error"}, {"code", m.code}, {"message", m.message}};
}

// ============================================================
// Framing
// ============================================================

std::vector<uint8_t> encode_frame(const std::string& payload) {
    auto len = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> frame(4 + payload.size());
    frame[0] = static_cast<uint8_t>((len >> 24) & 0xFF);
    frame[1] = static_cast<uint8_t>((len >> 16) & 0xFF);
    frame[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
    frame[3] = static_cast<uint8_t>(len & 0xFF);
    std::memcpy(frame.data() + 4, payload.data(), payload.size());
    return frame;
}

std::optional<uint32_t> decode_frame_header(const uint8_t* data) {
    uint32_t len = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);
    if (len > MAX_PAYLOAD_SIZE) return std::nullopt;
    return len;
}

} // namespace server
