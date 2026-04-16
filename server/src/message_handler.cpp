#include "server/message_handler.h"
#include "server/document_manager.h"
#include "server/tcp_server.h"
#include "collab/auth_manager.h"
#include "collab/access_control.h"
#include "collab/operation.h"

#include <cmath>
#include <format>

namespace server {

MessageHandler::MessageHandler(collab::AuthManager& auth,
                               collab::AccessControl& access,
                               DocumentManager& doc_manager,
                               TcpServer& tcp_server,
                               AsyncLogger& logger)
    : auth_(auth), access_(access), doc_manager_(doc_manager)
    , tcp_server_(tcp_server), logger_(logger) {}

void MessageHandler::handle_message(std::shared_ptr<ClientSession> session,
                                    const std::string& json_payload) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_payload);
    } catch (...) {
        send_error(session, "invalid_message", "Invalid JSON");
        return;
    }

    if (!j.contains("type") || !j["type"].is_string()) {
        send_error(session, "invalid_message", "Missing 'type' field");
        return;
    }

    auto type = parse_message_type(j["type"].get<std::string>());

    if (!check_auth(session, type)) return;

    try {
        switch (type) {
        case MessageType::AuthRequest:
            handle_auth_request(session, j);
            break;
        case MessageType::DocListRequest:
            handle_doc_list(session);
            break;
        case MessageType::DocCreateRequest:
            handle_doc_create(session, j);
            break;
        case MessageType::DocJoinRequest:
            handle_doc_join(session, j);
            break;
        case MessageType::DocLeaveRequest:
            handle_doc_leave(session, j);
            break;
        case MessageType::DocDeleteRequest:
            handle_doc_delete(session, j);
            break;
        case MessageType::DocShareRequest:
            handle_doc_share(session, j);
            break;
        case MessageType::Operation:
            handle_operation(session, j);
            break;
        case MessageType::CursorUpdate:
            handle_cursor_update(session, j);
            break;
        case MessageType::UndoRequest:
        case MessageType::RedoRequest:
        case MessageType::SnapshotListRequest:
        case MessageType::SnapshotViewRequest:
        case MessageType::SnapshotRestoreRequest:
            send_error(session, "not_implemented",
                       "This feature is not available in MVP");
            break;
        default:
            send_error(session, "invalid_message", "Unknown message type");
            break;
        }
    } catch (const nlohmann::json::exception& e) {
        send_error(session, "invalid_message",
                   std::format("Malformed message: {}", e.what()));
    }
}

bool MessageHandler::check_auth(const std::shared_ptr<ClientSession>& session,
                                MessageType type) {
    if (type == MessageType::AuthRequest) return true;
    if (!session->is_authenticated()) {
        send_error(session, "not_authenticated",
                   "Authentication required");
        return false;
    }
    return true;
}

// ============================================================
// Auth
// ============================================================

void MessageHandler::handle_auth_request(std::shared_ptr<ClientSession> session,
                                         const nlohmann::json& j) {
    auto msg = j.get<AuthRequestMsg>();

    if (msg.action != "login" && msg.action != "register") {
        send_error(session, "invalid_message", "Invalid action");
        return;
    }

    collab::AuthResult result;
    if (msg.action == "register") {
        result = auth_.register_user(msg.username, msg.password);
    } else {
        result = auth_.login(msg.username, msg.password);
    }

    if (result.success) {
        session->set_auth(result.userId, msg.username,
                          generate_color(result.userId));
        session->send(serialize(AuthResponseMsg{true, result.userId, ""}));
        logger_.info(std::format("User '{}' authenticated (id={})",
                                 msg.username, result.userId));
    } else {
        std::string error = result.error;
        if (error == "duplicate_username") error = "username_taken";
        session->send(serialize(AuthResponseMsg{false, 0, error}));
    }
}

// ============================================================
// Documents
// ============================================================

void MessageHandler::handle_doc_list(std::shared_ptr<ClientSession> session) {
    auto items = doc_manager_.list_documents(session->user_id());
    DocListResponseMsg resp;
    resp.documents.reserve(items.size());
    for (auto& item : items) {
        resp.documents.push_back({item.docId, item.title,
                                  item.role, item.onlineCount});
    }
    session->send(serialize(resp));
}

void MessageHandler::handle_doc_create(std::shared_ptr<ClientSession> session,
                                       const nlohmann::json& j) {
    auto msg = j.get<DocCreateRequestMsg>();
    auto docId = doc_manager_.create_document(msg.title, session->user_id());
    session->send(serialize(DocCreateResponseMsg{true, docId}));
    logger_.info(std::format("Document '{}' created (id={}) by user {}",
                             msg.title, docId, session->user_id()));
}

void MessageHandler::handle_doc_join(std::shared_ptr<ClientSession> session,
                                     const nlohmann::json& j) {
    auto msg = j.get<DocJoinRequestMsg>();

    if (!access_.can_read(msg.docId, session->user_id())) {
        session->send(serialize(DocJoinResponseMsg{
            .success = false, .error = "access_denied"}));
        return;
    }

    auto doc = doc_manager_.get_document(msg.docId);
    if (!doc) {
        session->send(serialize(DocJoinResponseMsg{
            .success = false, .error = "doc_not_found"}));
        return;
    }

    auto doc_session = doc_manager_.get_or_create_session(msg.docId);
    if (!doc_session) {
        session->send(serialize(DocJoinResponseMsg{
            .success = false, .error = "doc_not_found"}));
        return;
    }

    // M-3: reuse DocumentManager::role_to_string instead of duplicating
    auto role_str = doc_manager_.role_to_string(msg.docId, session->user_id());

    // Get current state before join
    auto content = doc->get_content();
    auto revision = doc->revision();

    // Get currently connected users
    auto connected = doc_session->connected_users();
    std::vector<UserInfo> users;
    for (auto& cu : connected) {
        users.push_back({cu.userId, cu.username, cu.color});
    }

    // Send join response first
    session->set_current_doc(msg.docId);
    session->send(serialize(DocJoinResponseMsg{
        .success = true, .docId = msg.docId, .title = doc->title(),
        .content = content, .revision = revision,
        .role = role_str, .users = users}));

    // Enqueue join command to document thread
    DocCommand cmd;
    cmd.type = DocCommand::Type::Join;
    cmd.userId = session->user_id();
    cmd.username = session->username();
    cmd.color = session->color();
    cmd.session = session;
    doc_session->enqueue_command(std::move(cmd));
}

void MessageHandler::handle_doc_leave(std::shared_ptr<ClientSession> session,
                                      const nlohmann::json& j) {
    auto msg = j.get<DocLeaveRequestMsg>();

    auto doc_session = doc_manager_.get_session(msg.docId);
    if (doc_session) {
        DocCommand cmd;
        cmd.type = DocCommand::Type::Leave;
        cmd.userId = session->user_id();
        cmd.username = session->username();
        doc_session->enqueue_command(std::move(cmd));
    }

    session->set_current_doc(0);
    session->send(serialize(DocLeaveResponseMsg{true}));
}

void MessageHandler::handle_doc_delete(std::shared_ptr<ClientSession> session,
                                       const nlohmann::json& j) {
    auto msg = j.get<DocDeleteRequestMsg>();

    if (!access_.can_delete(msg.docId, session->user_id())) {
        send_error(session, "access_denied",
                   "Only owner can delete documents");
        session->send(serialize(DocDeleteResponseMsg{false}));
        return;
    }

    // Notify subscribers
    auto doc_session = doc_manager_.get_session(msg.docId);
    if (doc_session) {
        DocCommand cmd;
        cmd.type = DocCommand::Type::Delete;
        doc_session->enqueue_command(std::move(cmd));
    }

    doc_manager_.delete_document(msg.docId);
    session->send(serialize(DocDeleteResponseMsg{true}));
    logger_.info(std::format("Document {} deleted by user {}",
                             msg.docId, session->user_id()));
}

void MessageHandler::handle_doc_share(std::shared_ptr<ClientSession> session,
                                      const nlohmann::json& j) {
    auto msg = j.get<DocShareRequestMsg>();

    if (!access_.can_share(msg.docId, session->user_id())) {
        send_error(session, "access_denied",
                   "Only owner can manage access");
        return;
    }

    auto target_id = auth_.find_user_id(msg.targetUsername);
    if (!target_id) {
        send_error(session, "user_not_found",
                   "Target user not found");
        return;
    }

    if (*target_id == session->user_id()) {
        send_error(session, "cannot_modify_own_role",
                   "Cannot modify own role");
        return;
    }

    if (msg.role == "none") {
        access_.revoke(msg.docId, *target_id);
    } else if (msg.role == "editor") {
        access_.grant(msg.docId, *target_id, collab::Role::Editor);
    } else if (msg.role == "viewer") {
        access_.grant(msg.docId, *target_id, collab::Role::Viewer);
    } else {
        send_error(session, "invalid_message", "Invalid role");
        return;
    }

    session->send(serialize(DocShareResponseMsg{true}));

    // Notify target user if online
    auto target_session = tcp_server_.find_session_by_user_id(*target_id);
    if (target_session && target_session->current_doc_id() == msg.docId) {
        target_session->send(serialize(RoleChangedMsg{msg.docId, msg.role}));
    }
}

// ============================================================
// Operations
// ============================================================

void MessageHandler::handle_operation(std::shared_ptr<ClientSession> session,
                                      const nlohmann::json& j) {
    auto msg = j.get<OperationMsg>();

    if (session->current_doc_id() != msg.docId) {
        send_error(session, "not_in_document",
                   "Not in this document");
        return;
    }

    if (!access_.can_edit(msg.docId, session->user_id())) {
        send_error(session, "access_denied",
                   "No edit permission");
        return;
    }

    // M-4: validate ops array size per spec (max 1000)
    if (msg.ops.size() > 1000) {
        send_error(session, "invalid_message", "Too many operations (max 1000)");
        return;
    }

    auto doc_session = doc_manager_.get_session(msg.docId);
    if (!doc_session) {
        send_error(session, "doc_not_found", "Document not active");
        return;
    }

    for (auto& op_entry : msg.ops) {
        DocCommand cmd;
        cmd.type = DocCommand::Type::Operation;
        if (op_entry.op == "insert") {
            cmd.operation = collab::make_insert(
                op_entry.pos, op_entry.text,
                session->user_id(), msg.revision);
        } else {
            cmd.operation = collab::make_delete(
                op_entry.pos, op_entry.len, "",
                session->user_id(), msg.revision);
        }
        cmd.userId = session->user_id();
        cmd.username = session->username();
        doc_session->enqueue_command(std::move(cmd));
    }
}

// ============================================================
// Cursors
// ============================================================

void MessageHandler::handle_cursor_update(std::shared_ptr<ClientSession> session,
                                          const nlohmann::json& j) {
    auto msg = j.get<CursorUpdateMsg>();

    if (session->current_doc_id() != msg.docId) {
        return; // Silently ignore
    }

    auto doc_session = doc_manager_.get_session(msg.docId);
    if (!doc_session) return;

    CursorState state;
    state.userId = session->user_id();
    state.username = session->username();
    state.color = session->color();
    state.position = msg.position;
    state.selectionStart = msg.selectionStart;
    state.selectionEnd = msg.selectionEnd;

    doc_manager_.update_cursor(msg.docId, std::move(state));
}

// ============================================================
// Helpers
// ============================================================

void MessageHandler::send_error(std::shared_ptr<ClientSession> session,
                                const std::string& code,
                                const std::string& message) {
    session->send(serialize(ErrorMsg{code, message}));
    // M-6: close connection after 3 malformed messages (per spec docs/07)
    if (code == "invalid_message" && session->increment_error_count() >= 3) {
        logger_.warn(std::format("Closing session {} after 3 malformed messages",
                                  session->session_id()));
        session->close();
    }
}

std::string MessageHandler::generate_color(uint32_t userId) {
    // Golden ratio hue spacing for visually distinct colors
    constexpr double golden = 0.618033988749895;
    double hue = std::fmod(userId * golden, 1.0) * 360.0;
    double s = 0.7, l = 0.5;

    // HSL to RGB
    auto hue2rgb = [](double p, double q, double t) -> double {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0/6) return p + (q - p) * 6 * t;
        if (t < 1.0/2) return q;
        if (t < 2.0/3) return p + (q - p) * (2.0/3 - t) * 6;
        return p;
    };

    double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    double p = 2 * l - q;
    double h = hue / 360.0;

    int r = static_cast<int>(hue2rgb(p, q, h + 1.0/3) * 255);
    int g = static_cast<int>(hue2rgb(p, q, h) * 255);
    int b = static_cast<int>(hue2rgb(p, q, h - 1.0/3) * 255);

    return std::format("#{:02X}{:02X}{:02X}", r, g, b);
}

} // namespace server
