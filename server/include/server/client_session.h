#pragma once

#include "collab_protocol/protocol.h"

#include <boost/asio.hpp>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace server {

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    using MessageCallback = std::function<void(std::shared_ptr<ClientSession>,
                                               const std::string&)>;
    using DisconnectCallback = std::function<void(std::shared_ptr<ClientSession>)>;

    ClientSession(boost::asio::ip::tcp::socket socket,
                  MessageCallback on_message,
                  DisconnectCallback on_disconnect);

    void start();
    void send(const std::string& json_payload);
    void close();

    void set_auth(uint32_t userId, const std::string& username,
                  const std::string& color);

    bool is_authenticated() const { std::lock_guard lock(state_mutex_); return authenticated_; }
    uint32_t user_id() const { std::lock_guard lock(state_mutex_); return userId_; }
    std::string username() const { std::lock_guard lock(state_mutex_); return username_; }
    std::string color() const { std::lock_guard lock(state_mutex_); return color_; }

    void set_current_doc(uint32_t docId) { std::lock_guard lock(state_mutex_); currentDocId_ = docId; }
    uint32_t current_doc_id() const { std::lock_guard lock(state_mutex_); return currentDocId_; }
    uint64_t session_id() const { return session_id_; }

    int increment_error_count() { return error_count_.fetch_add(1, std::memory_order_relaxed) + 1; }

private:
    void do_read_header();
    void do_read_body(uint32_t body_length);
    void do_write();

    boost::asio::ip::tcp::socket socket_;
    MessageCallback on_message_;
    DisconnectCallback on_disconnect_;

    std::array<uint8_t, 4> header_buf_{};
    std::vector<uint8_t> body_buf_;

    std::mutex write_mutex_;
    std::deque<std::vector<uint8_t>> write_queue_;
    bool writing_{false};

    mutable std::mutex state_mutex_;
    bool authenticated_{false};
    uint32_t userId_{0};
    std::string username_;
    std::string color_;
    uint32_t currentDocId_{0};

    std::atomic<bool> disconnected_{false};
    std::atomic<int> error_count_{0};

    uint64_t session_id_;
    static std::atomic<uint64_t> next_session_id_;
};

} // namespace server
