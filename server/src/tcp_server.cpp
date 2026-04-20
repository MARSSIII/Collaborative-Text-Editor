#include "server/tcp_server.h"

namespace server {

TcpServer::TcpServer(boost::asio::io_context& io_ctx, uint16_t port,
                     ClientSession::MessageCallback on_message,
                     ClientSession::DisconnectCallback on_disconnect)
    : acceptor_(io_ctx, boost::asio::ip::tcp::endpoint(
          boost::asio::ip::tcp::v4(), port))
    , on_message_(std::move(on_message))
    , on_disconnect_(std::move(on_disconnect)) {}

void TcpServer::start() {
    do_accept();
}

void TcpServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
}

void TcpServer::broadcast_all(const std::string& json_payload) {
    std::lock_guard lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session->send(json_payload);
    }
}

std::shared_ptr<ClientSession> TcpServer::find_session_by_user_id(uint32_t userId) {
    std::lock_guard lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        if (session->is_authenticated() && session->user_id() == userId) {
            return session;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<ClientSession>> TcpServer::all_sessions() {
    std::lock_guard lock(sessions_mutex_);
    std::vector<std::shared_ptr<ClientSession>> result;
    result.reserve(sessions_.size());
    for (auto& [id, session] : sessions_) {
        result.push_back(session);
    }
    return result;
}

void TcpServer::remove_session(uint64_t session_id) {
    std::lock_guard lock(sessions_mutex_);
    sessions_.erase(session_id);
}

void TcpServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (ec) return;

            auto session = std::make_shared<ClientSession>(
                std::move(socket), on_message_, on_disconnect_);

            {
                std::lock_guard lock(sessions_mutex_);
                sessions_[session->session_id()] = session;
            }

            session->start();
            do_accept();
        });
}

}
