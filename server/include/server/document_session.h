#pragma once

#include "collab/document.h"
#include "collab/mpsc_queue.h"
#include "collab/operation.h"
#include "server/client_session.h"
#include "server/async_logger.h"
#include "collab_protocol/protocol.h"

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace server {

struct ConnectedUser {
    uint32_t userId;
    std::string username;
    std::string color;
    std::weak_ptr<ClientSession> session;
};

struct CursorState {
    uint32_t userId;
    std::string username;
    std::string color;
    uint32_t position;
    std::optional<uint32_t> selectionStart;
    std::optional<uint32_t> selectionEnd;
};

struct DocCommand {
    enum class Type { Operation, Join, Leave, Delete, Shutdown };
    Type type;
    collab::Operation operation{collab::Operation::Type::Insert, 0, "", 0, 0, 0};
    uint32_t userId{0};
    std::string username;
    std::string color;
    std::weak_ptr<ClientSession> session;
};

class DocumentSession {
public:
    DocumentSession(std::shared_ptr<collab::Document> doc,
                    AsyncLogger& logger);
    ~DocumentSession();

    DocumentSession(const DocumentSession&) = delete;
    DocumentSession& operator=(const DocumentSession&) = delete;

    void enqueue_command(DocCommand cmd);
    void start();
    void request_stop();

    std::shared_ptr<collab::Document> document() const { return doc_; }
    std::vector<ConnectedUser> connected_users() const;
    bool has_subscribers() const;

private:
    void run(std::stop_token stop);
    void process_command(const DocCommand& cmd);
    void handle_join(const DocCommand& cmd);
    void handle_leave(const DocCommand& cmd);
    void handle_operation(const DocCommand& cmd);
    void handle_delete();

    void broadcast(const std::string& json_payload, uint32_t excludeUserId = 0);
    void send_to_user(uint32_t userId, const std::string& json_payload);

    std::shared_ptr<collab::Document> doc_;
    AsyncLogger& logger_;

    collab::MPSCQueue<DocCommand> command_queue_;
    std::jthread thread_;

    mutable std::mutex connected_users_mutex_;
    std::unordered_map<uint32_t, ConnectedUser> connected_users_;
};

}
