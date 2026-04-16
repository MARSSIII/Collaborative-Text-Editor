#include "server/client_session.h"

namespace server {

std::atomic<uint64_t> ClientSession::next_session_id_{1};

ClientSession::ClientSession(boost::asio::ip::tcp::socket socket,
                             MessageCallback on_message,
                             DisconnectCallback on_disconnect)
    : socket_(std::move(socket))
    , on_message_(std::move(on_message))
    , on_disconnect_(std::move(on_disconnect))
    , session_id_(next_session_id_.fetch_add(1, std::memory_order_relaxed)) {}

void ClientSession::start() {
    do_read_header();
}

void ClientSession::send(const std::string& json_payload) {
    if (disconnected_.load(std::memory_order_relaxed)) return;  // C-3

    auto frame = encode_frame(json_payload);

    std::lock_guard lock(write_mutex_);
    write_queue_.push_back(std::move(frame));
    if (!writing_) {
        writing_ = true;
        do_write();
    }
}

void ClientSession::close() {
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

void ClientSession::set_auth(uint32_t userId, const std::string& username,
                             const std::string& color) {
    std::lock_guard lock(state_mutex_);  // C-2
    userId_ = userId;
    username_ = username;
    color_ = color;
    authenticated_ = true;
}

void ClientSession::do_read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(header_buf_),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                if (!disconnected_.exchange(true))  // C-3
                    on_disconnect_(self);
                return;
            }
            auto payload_len = decode_frame_header(header_buf_.data());
            if (!payload_len) {
                close();
                if (!disconnected_.exchange(true))  // C-3
                    on_disconnect_(self);
                return;
            }
            do_read_body(*payload_len);
        });
}

void ClientSession::do_read_body(uint32_t body_length) {
    body_buf_.resize(body_length);
    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(body_buf_),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                if (!disconnected_.exchange(true))  // C-3
                    on_disconnect_(self);
                return;
            }
            std::string payload(body_buf_.begin(), body_buf_.end());
            on_message_(self, payload);
            do_read_header();
        });
}

void ClientSession::do_write() {
    // Must be called with write_mutex_ held
    if (write_queue_.empty()) {
        writing_ = false;
        return;
    }

    auto self = shared_from_this();
    auto& front = write_queue_.front();
    boost::asio::async_write(socket_,
        boost::asio::buffer(front),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                if (!disconnected_.exchange(true))  // C-3
                    on_disconnect_(self);
                return;
            }
            std::lock_guard lock(write_mutex_);
            write_queue_.pop_front();  // M-1: O(1) instead of O(n)
            if (!write_queue_.empty()) {
                do_write();
            } else {
                writing_ = false;
            }
        });
}

} // namespace server
