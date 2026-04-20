#include "test_client.h"

#include "collab_protocol/protocol.h"

#include <boost/asio.hpp>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace test {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

TestClient::TestClient(uint16_t port) : sock_(io_) {
    tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), port);
    sock_.connect(ep);
}

TestClient::~TestClient() {
    close();
}

void TestClient::close() {
    if (connected_) {
        boost::system::error_code ec;
        sock_.shutdown(tcp::socket::shutdown_both, ec);
        sock_.close(ec);
        connected_ = false;
    }
}

bool TestClient::is_connected() const {
    if (!connected_) return false;
    boost::system::error_code ec;
    sock_.remote_endpoint(ec);
    if (ec) {
        connected_ = false;
        return false;
    }
    return true;
}

void TestClient::send(const nlohmann::json& msg) {
    auto payload = msg.dump();
    auto frame = server::encode_frame(payload);
    boost::system::error_code ec;
    asio::write(sock_, asio::buffer(frame), ec);
    if (ec) {
        connected_ = false;
        throw std::runtime_error("send failed: " + ec.message());
    }
}

void TestClient::send_raw_frame(const std::string& payload) {
    auto frame = server::encode_frame(payload);
    boost::system::error_code ec;
    asio::write(sock_, asio::buffer(frame), ec);
    if (ec) {
        connected_ = false;
    }
}

std::optional<nlohmann::json> TestClient::recv(std::chrono::milliseconds timeout) {
    if (!pending_.empty()) {
        auto msg = std::move(pending_.front());
        pending_.pop_front();
        return msg;
    }
    return recv_raw_(timeout);
}

std::optional<nlohmann::json> TestClient::recv_raw_(std::chrono::milliseconds timeout) {
    if (!connected_) return std::nullopt;

    auto deadline = std::chrono::steady_clock::now() + timeout;
    std::array<uint8_t, 4> header{};
    boost::system::error_code read_ec;
    bool read_complete = false;
    std::size_t got = 0;

    io_.restart();

    auto try_read_header = [&]() {
        asio::async_read(sock_, asio::buffer(header.data() + got, 4 - got),
            [&](boost::system::error_code ec, std::size_t n) {
                read_ec = ec;
                got += n;
                if (!ec && got == 4) read_complete = true;
            });
    };
    try_read_header();

    while (!read_complete && !read_ec) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            sock_.cancel();
            io_.run();
            return std::nullopt;
        }
        io_.run_one_for(deadline - now);
    }

    if (read_ec) {
        if (read_ec == asio::error::eof || read_ec == asio::error::connection_reset) {
            connected_ = false;
        }
        return std::nullopt;
    }

    auto maybe_len = server::decode_frame_header(header.data());
    if (!maybe_len) return std::nullopt;
    std::vector<uint8_t> body(*maybe_len);

    read_complete = false;
    got = 0;
    io_.restart();

    auto try_read_body = [&]() {
        asio::async_read(sock_, asio::buffer(body.data() + got, body.size() - got),
            [&](boost::system::error_code ec, std::size_t n) {
                read_ec = ec;
                got += n;
                if (!ec && got == body.size()) read_complete = true;
            });
    };
    try_read_body();

    while (!read_complete && !read_ec) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            sock_.cancel();
            io_.run();
            return std::nullopt;
        }
        io_.run_one_for(deadline - now);
    }

    if (read_ec) {
        if (read_ec == asio::error::eof || read_ec == asio::error::connection_reset) {
            connected_ = false;
        }
        return std::nullopt;
    }

    try {
        return nlohmann::json::parse(std::string(body.begin(), body.end()));
    } catch (...) {
        return std::nullopt;
    }
}

nlohmann::json TestClient::recv_until_type(const std::string& type,
                                           std::chrono::milliseconds total_timeout) {
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->value("type", "") == type) {
            auto msg = std::move(*it);
            pending_.erase(it);
            return msg;
        }
    }
    auto deadline = std::chrono::steady_clock::now() + total_timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) break;
        auto msg = recv_raw_(remaining);
        if (!msg) return {};
        if (msg->value("type", "") == type) return *msg;
        pending_.push_back(std::move(*msg));
    }
    return {};
}

uint32_t TestClient::register_and_login(std::string username, std::string password) {
    send({
        {"type", "auth_request"},
        {"action", "register"},
        {"username", username},
        {"password", password},
    });
    auto resp = recv(2s);
    if (!resp || !resp->value("success", false)) {
        throw std::runtime_error("register failed: " +
            (resp ? resp->dump() : std::string("no response")));
    }
    return resp->at("userId").get<uint32_t>();
}

uint32_t TestClient::login(std::string username, std::string password) {
    send({
        {"type", "auth_request"},
        {"action", "login"},
        {"username", username},
        {"password", password},
    });
    auto resp = recv(2s);
    if (!resp || !resp->value("success", false)) {
        throw std::runtime_error("login failed: " +
            (resp ? resp->dump() : std::string("no response")));
    }
    return resp->at("userId").get<uint32_t>();
}

uint32_t TestClient::create_document(const std::string& title) {
    send({{"type", "doc_create_request"}, {"title", title}});
    auto resp = recv(2s);
    if (!resp || !resp->value("success", false)) {
        throw std::runtime_error("doc_create failed: " +
            (resp ? resp->dump() : std::string("no response")));
    }
    return resp->at("docId").get<uint32_t>();
}

nlohmann::json TestClient::doc_list() {
    send({{"type", "doc_list_request"}});
    auto resp = recv(2s);
    if (!resp) throw std::runtime_error("doc_list: no response");
    return *resp;
}

nlohmann::json TestClient::join_document(uint32_t doc_id) {
    send({{"type", "doc_join_request"}, {"docId", doc_id}});
    auto resp = recv_until_type("doc_join_response", 2s);
    if (resp.is_null()) throw std::runtime_error("doc_join: no response");
    return resp;
}

void TestClient::leave_document(uint32_t doc_id) {
    send({{"type", "doc_leave_request"}, {"docId", doc_id}});
    (void)recv_until_type("doc_leave_response", 2s);
}

void TestClient::share_document(uint32_t doc_id, const std::string& target_user,
                                const std::string& role) {
    send({
        {"type", "doc_share_request"},
        {"docId", doc_id},
        {"targetUsername", target_user},
        {"role", role},
    });
    (void)recv_until_type("doc_share_response", 2s);
}

void TestClient::send_insert(uint32_t doc_id, uint32_t revision, uint32_t pos,
                             const std::string& text) {
    nlohmann::json op = {{"op", "insert"}, {"pos", pos}, {"text", text}};
    send({
        {"type", "operation"},
        {"docId", doc_id},
        {"revision", revision},
        {"ops", nlohmann::json::array({op})},
    });
}

void TestClient::send_delete(uint32_t doc_id, uint32_t revision, uint32_t pos,
                             uint32_t len) {
    nlohmann::json op = {{"op", "delete"}, {"pos", pos}, {"len", len}};
    send({
        {"type", "operation"},
        {"docId", doc_id},
        {"revision", revision},
        {"ops", nlohmann::json::array({op})},
    });
}

}
