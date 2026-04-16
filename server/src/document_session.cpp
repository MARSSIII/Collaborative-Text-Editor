#include "server/document_session.h"

#include <format>

namespace server {

DocumentSession::DocumentSession(std::shared_ptr<collab::Document> doc,
                                 AsyncLogger& logger)
    : doc_(std::move(doc)), logger_(logger) {}

DocumentSession::~DocumentSession() {
    request_stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void DocumentSession::enqueue_command(DocCommand cmd) {
    command_queue_.enqueue(std::move(cmd));
}

void DocumentSession::start() {
    thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
}

void DocumentSession::request_stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
    }
}

std::vector<ConnectedUser> DocumentSession::connected_users() const {
    std::lock_guard lock(connected_users_mutex_);  // C-1
    std::vector<ConnectedUser> result;
    result.reserve(connected_users_.size());
    for (auto& [id, user] : connected_users_) {
        result.push_back(user);
    }
    return result;
}

bool DocumentSession::has_subscribers() const {
    return doc_->subscriber_count() > 0;
}

void DocumentSession::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        bool processed = false;
        while (auto cmd = command_queue_.try_dequeue()) {
            process_command(*cmd);
            processed = true;
        }
        if (!processed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    // Drain remaining commands
    while (auto cmd = command_queue_.try_dequeue()) {
        process_command(*cmd);
    }
}

void DocumentSession::process_command(const DocCommand& cmd) {
    switch (cmd.type) {
    case DocCommand::Type::Join:
        handle_join(cmd);
        break;
    case DocCommand::Type::Leave:
        handle_leave(cmd);
        break;
    case DocCommand::Type::Operation:
        handle_operation(cmd);
        break;
    case DocCommand::Type::Delete:
        handle_delete();
        break;
    case DocCommand::Type::Shutdown:
        break;
    }
}

void DocumentSession::handle_join(const DocCommand& cmd) {
    {
        std::lock_guard lock(connected_users_mutex_);  // C-1
        connected_users_[cmd.userId] = {
            cmd.userId, cmd.username, cmd.color, cmd.session
        };
    }
    doc_->subscribe(cmd.userId);

    broadcast(serialize(UserJoinedMsg{
        doc_->id(), cmd.userId, cmd.username, cmd.color
    }), cmd.userId);

    logger_.info(std::format("User '{}' joined document {}",
                             cmd.username, doc_->id()));
}

void DocumentSession::handle_leave(const DocCommand& cmd) {
    {
        std::lock_guard lock(connected_users_mutex_);  // C-1
        connected_users_.erase(cmd.userId);
    }
    doc_->unsubscribe(cmd.userId);

    broadcast(serialize(UserLeftMsg{
        doc_->id(), cmd.userId, cmd.username
    }));

    logger_.info(std::format("User '{}' left document {}",
                             cmd.username, doc_->id()));
}

void DocumentSession::handle_operation(const DocCommand& cmd) {
    auto result = doc_->apply_with_ot(cmd.operation);

    if (!result.success) {
        send_to_user(cmd.userId,
            serialize(ErrorMsg{"revision_too_old", result.error}));
        return;
    }

    // Ack to author
    send_to_user(cmd.userId,
        serialize(OperationAckMsg{doc_->id(), doc_->revision()}));

    // Broadcast to others
    auto& op = result.transformed_op;
    OpEntry entry;
    if (op.type == collab::Operation::Type::Insert) {
        entry = {.op = "insert", .pos = op.position, .text = op.text};
    } else {
        entry = {.op = "delete", .pos = op.position, .len = op.length};
    }

    broadcast(serialize(OperationBroadcastMsg{
        doc_->id(), cmd.userId, cmd.username,
        doc_->revision(), {entry}
    }), cmd.userId);
}

void DocumentSession::handle_delete() {
    broadcast(serialize(DocDeletedMsg{doc_->id()}));
    {
        std::lock_guard lock(connected_users_mutex_);  // C-1
        connected_users_.clear();
    }
    logger_.info(std::format("Document {} deleted, all users disconnected",
                             doc_->id()));
}

void DocumentSession::broadcast(const std::string& json_payload,
                                uint32_t excludeUserId) {
    // C-1: copy weak_ptrs under lock, send outside lock
    std::vector<std::weak_ptr<ClientSession>> targets;
    {
        std::lock_guard lock(connected_users_mutex_);
        targets.reserve(connected_users_.size());
        for (auto& [userId, user] : connected_users_) {
            if (userId != excludeUserId) {
                targets.push_back(user.session);
            }
        }
    }
    for (auto& wp : targets) {
        if (auto session = wp.lock()) {
            session->send(json_payload);
        }
    }
}

void DocumentSession::send_to_user(uint32_t userId,
                                   const std::string& json_payload) {
    std::weak_ptr<ClientSession> wp;
    {
        std::lock_guard lock(connected_users_mutex_);  // C-1
        auto it = connected_users_.find(userId);
        if (it != connected_users_.end()) {
            wp = it->second.session;
        }
    }
    if (auto session = wp.lock()) {
        session->send(json_payload);
    }
}

} // namespace server
