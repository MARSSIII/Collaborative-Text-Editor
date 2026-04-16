#pragma once

#include "server/tcp_server.h"
#include "server/protocol.h"
#include "server/async_logger.h"

#include <memory>
#include <string>

namespace server {

class DocumentManager;

} // namespace server

namespace collab {
class AuthManager;
class AccessControl;
} // namespace collab

namespace server {

class MessageHandler {
public:
    MessageHandler(collab::AuthManager& auth,
                   collab::AccessControl& access,
                   DocumentManager& doc_manager,
                   TcpServer& tcp_server,
                   AsyncLogger& logger);

    void handle_message(std::shared_ptr<ClientSession> session,
                        const std::string& json_payload);

private:
    bool check_auth(const std::shared_ptr<ClientSession>& session,
                    MessageType type);

    void handle_auth_request(std::shared_ptr<ClientSession> session,
                             const nlohmann::json& j);
    void handle_doc_list(std::shared_ptr<ClientSession> session);
    void handle_doc_create(std::shared_ptr<ClientSession> session,
                           const nlohmann::json& j);
    void handle_doc_join(std::shared_ptr<ClientSession> session,
                         const nlohmann::json& j);
    void handle_doc_leave(std::shared_ptr<ClientSession> session,
                          const nlohmann::json& j);
    void handle_doc_delete(std::shared_ptr<ClientSession> session,
                           const nlohmann::json& j);
    void handle_doc_share(std::shared_ptr<ClientSession> session,
                          const nlohmann::json& j);
    void handle_operation(std::shared_ptr<ClientSession> session,
                          const nlohmann::json& j);
    void handle_cursor_update(std::shared_ptr<ClientSession> session,
                              const nlohmann::json& j);

    void send_error(std::shared_ptr<ClientSession> session,
                    const std::string& code, const std::string& message);

    static std::string generate_color(uint32_t userId);

    collab::AuthManager& auth_;
    collab::AccessControl& access_;
    DocumentManager& doc_manager_;
    TcpServer& tcp_server_;
    AsyncLogger& logger_;
};

} // namespace server
