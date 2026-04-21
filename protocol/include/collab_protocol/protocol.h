#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace server {

enum class MessageType {
    AuthRequest, AuthResponse,
    DocListRequest, DocListResponse,
    DocCreateRequest, DocCreateResponse,
    DocJoinRequest, DocJoinResponse,
    DocLeaveRequest, DocLeaveResponse,
    DocDeleteRequest, DocDeleteResponse,
    DocShareRequest, DocShareResponse,
    Operation, OperationAck, OperationBroadcast,
    CursorUpdate, CursorBroadcast,
    UserJoined, UserLeft,
    RoleChanged, DocDeleted, ServerShutdown, Error,
    UndoRequest, RedoRequest,
    SnapshotListRequest, SnapshotListResponse,
    SnapshotViewRequest, SnapshotViewResponse,
    SnapshotRestoreRequest, SnapshotRestoreResponse,
    RateLimit,
    Unknown
};

MessageType parse_message_type(const std::string& type_str);
std::string message_type_to_string(MessageType type);

struct AuthRequestMsg {
    std::string action;
    std::string username;
    std::string password;
};

struct DocListRequestMsg {};

struct DocCreateRequestMsg {
    std::string title;
};

struct DocJoinRequestMsg {
    uint32_t docId;
};

struct DocLeaveRequestMsg {
    uint32_t docId;
};

struct DocDeleteRequestMsg {
    uint32_t docId;
};

struct DocShareRequestMsg {
    uint32_t docId;
    std::string targetUsername;
    std::string role;
};

struct OpEntry {
    std::string op;
    uint32_t pos;
    std::string text;
    uint32_t len{0};
};

struct OperationMsg {
    uint32_t docId;
    uint32_t revision;
    std::vector<OpEntry> ops;
};

struct CursorUpdateMsg {
    uint32_t docId;
    uint32_t position;
    std::optional<uint32_t> selectionStart;
    std::optional<uint32_t> selectionEnd;
};

struct AuthResponseMsg {
    bool success;
    uint32_t userId{0};
    std::string error;
};

struct DocListEntry {
    uint32_t docId;
    std::string title;
    std::string role;
    int onlineCount;
};

struct DocListResponseMsg {
    std::vector<DocListEntry> documents;
};

struct DocCreateResponseMsg {
    bool success;
    uint32_t docId{0};
};

struct UserInfo {
    uint32_t userId;
    std::string username;
    std::string color;
};

struct DocJoinResponseMsg {
    bool success;
    uint32_t docId{0};
    std::string title;
    std::string content;
    uint32_t revision{0};
    std::string role;
    std::vector<UserInfo> users;
    std::string error;
};

struct DocLeaveResponseMsg {
    bool success;
};

struct DocDeleteResponseMsg {
    bool success;
};

struct DocShareResponseMsg {
    bool success;
};

struct OperationAckMsg {
    uint32_t docId;
    uint32_t revision;
};

struct OperationBroadcastMsg {
    uint32_t docId;
    uint32_t userId;
    std::string username;
    uint32_t revision;
    std::vector<OpEntry> ops;
};

struct CursorInfo {
    uint32_t userId;
    std::string username;
    std::string color;
    uint32_t position;
    std::optional<uint32_t> selectionStart;
    std::optional<uint32_t> selectionEnd;
};

struct CursorBroadcastMsg {
    uint32_t docId;
    std::vector<CursorInfo> cursors;
};

struct UserJoinedMsg {
    uint32_t docId;
    uint32_t userId;
    std::string username;
    std::string color;
};

struct UserLeftMsg {
    uint32_t docId;
    uint32_t userId;
    std::string username;
};

struct RoleChangedMsg {
    uint32_t docId;
    std::string newRole;
};

struct DocDeletedMsg {
    uint32_t docId;
};

struct ServerShutdownMsg {
    std::string message;
};

struct ErrorMsg {
    std::string code;
    std::string message;
};

void from_json(const nlohmann::json& j, AuthRequestMsg& m);
void from_json(const nlohmann::json& j, DocCreateRequestMsg& m);
void from_json(const nlohmann::json& j, DocJoinRequestMsg& m);
void from_json(const nlohmann::json& j, DocLeaveRequestMsg& m);
void from_json(const nlohmann::json& j, DocDeleteRequestMsg& m);
void from_json(const nlohmann::json& j, DocShareRequestMsg& m);
void from_json(const nlohmann::json& j, OpEntry& m);
void from_json(const nlohmann::json& j, OperationMsg& m);
void from_json(const nlohmann::json& j, CursorUpdateMsg& m);

void to_json(nlohmann::json& j, const AuthRequestMsg& m);
void to_json(nlohmann::json& j, const DocListRequestMsg& m);
void to_json(nlohmann::json& j, const DocCreateRequestMsg& m);
void to_json(nlohmann::json& j, const DocJoinRequestMsg& m);
void to_json(nlohmann::json& j, const DocLeaveRequestMsg& m);
void to_json(nlohmann::json& j, const DocDeleteRequestMsg& m);
void to_json(nlohmann::json& j, const DocShareRequestMsg& m);
void to_json(nlohmann::json& j, const OperationMsg& m);
void to_json(nlohmann::json& j, const CursorUpdateMsg& m);

void from_json(const nlohmann::json& j, AuthResponseMsg& m);
void from_json(const nlohmann::json& j, DocListEntry& m);
void from_json(const nlohmann::json& j, DocListResponseMsg& m);
void from_json(const nlohmann::json& j, DocCreateResponseMsg& m);
void from_json(const nlohmann::json& j, UserInfo& m);
void from_json(const nlohmann::json& j, DocJoinResponseMsg& m);
void from_json(const nlohmann::json& j, DocLeaveResponseMsg& m);
void from_json(const nlohmann::json& j, DocDeleteResponseMsg& m);
void from_json(const nlohmann::json& j, DocShareResponseMsg& m);
void from_json(const nlohmann::json& j, OperationAckMsg& m);
void from_json(const nlohmann::json& j, OperationBroadcastMsg& m);
void from_json(const nlohmann::json& j, CursorInfo& m);
void from_json(const nlohmann::json& j, CursorBroadcastMsg& m);
void from_json(const nlohmann::json& j, UserJoinedMsg& m);
void from_json(const nlohmann::json& j, UserLeftMsg& m);
void from_json(const nlohmann::json& j, RoleChangedMsg& m);
void from_json(const nlohmann::json& j, DocDeletedMsg& m);
void from_json(const nlohmann::json& j, ServerShutdownMsg& m);
void from_json(const nlohmann::json& j, ErrorMsg& m);

void to_json(nlohmann::json& j, const AuthResponseMsg& m);
void to_json(nlohmann::json& j, const DocListEntry& m);
void to_json(nlohmann::json& j, const DocListResponseMsg& m);
void to_json(nlohmann::json& j, const DocCreateResponseMsg& m);
void to_json(nlohmann::json& j, const UserInfo& m);
void to_json(nlohmann::json& j, const DocJoinResponseMsg& m);
void to_json(nlohmann::json& j, const DocLeaveResponseMsg& m);
void to_json(nlohmann::json& j, const DocDeleteResponseMsg& m);
void to_json(nlohmann::json& j, const DocShareResponseMsg& m);
void to_json(nlohmann::json& j, const OperationAckMsg& m);
void to_json(nlohmann::json& j, const OpEntry& m);
void to_json(nlohmann::json& j, const OperationBroadcastMsg& m);
void to_json(nlohmann::json& j, const CursorInfo& m);
void to_json(nlohmann::json& j, const CursorBroadcastMsg& m);
void to_json(nlohmann::json& j, const UserJoinedMsg& m);
void to_json(nlohmann::json& j, const UserLeftMsg& m);
void to_json(nlohmann::json& j, const RoleChangedMsg& m);
void to_json(nlohmann::json& j, const DocDeletedMsg& m);
void to_json(nlohmann::json& j, const ServerShutdownMsg& m);
void to_json(nlohmann::json& j, const ErrorMsg& m);

template<typename T>
std::string serialize(const T& msg) {
    nlohmann::json j = msg;
    return j.dump();
}

constexpr uint32_t MAX_PAYLOAD_SIZE = 16 * 1024 * 1024;

std::vector<uint8_t> encode_frame(const std::string& payload);

std::optional<uint32_t> decode_frame_header(const uint8_t* data);

}
