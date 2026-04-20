#pragma once

#include "server/client_session.h"

#include <boost/asio.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace server {

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_ctx, uint16_t port,
              ClientSession::MessageCallback on_message,
              ClientSession::DisconnectCallback on_disconnect);

    void start();
    void stop();
    void broadcast_all(const std::string& json_payload);
    std::shared_ptr<ClientSession> find_session_by_user_id(uint32_t userId);
    std::vector<std::shared_ptr<ClientSession>> all_sessions();
    void remove_session(uint64_t session_id);

    uint16_t local_port() const { return acceptor_.local_endpoint().port(); }

private:
    void do_accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    ClientSession::MessageCallback on_message_;
    ClientSession::DisconnectCallback on_disconnect_;

    mutable std::mutex sessions_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<ClientSession>> sessions_;
};

} // namespace server
