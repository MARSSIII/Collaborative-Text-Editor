#pragma once

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace test {

class TestClient {
public:
    explicit TestClient(uint16_t port);
    ~TestClient();

    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;

    void send(const nlohmann::json& msg);

    // Receives one framed JSON message. Returns std::nullopt on timeout or disconnect.
    std::optional<nlohmann::json> recv(std::chrono::milliseconds timeout);

    // Sends a raw (unframed) byte sequence - used to simulate a malformed frame
    // payload that is still length-prefixed correctly but is not valid JSON.
    void send_raw_frame(const std::string& payload);

    bool is_connected() const;
    void close();

    uint32_t register_and_login(std::string username, std::string password);
    uint32_t login(std::string username, std::string password);

    uint32_t create_document(const std::string& title);
    nlohmann::json doc_list();
    nlohmann::json join_document(uint32_t doc_id);
    void leave_document(uint32_t doc_id);
    void share_document(uint32_t doc_id, const std::string& target_user,
                        const std::string& role);

    void send_insert(uint32_t doc_id, uint32_t revision, uint32_t pos,
                     const std::string& text);
    void send_delete(uint32_t doc_id, uint32_t revision, uint32_t pos,
                     uint32_t len);

    nlohmann::json recv_until_type(const std::string& type,
                                   std::chrono::milliseconds total_timeout);

private:
    std::optional<nlohmann::json> recv_raw_(std::chrono::milliseconds timeout);

    boost::asio::io_context io_;
    boost::asio::ip::tcp::socket sock_;
    std::deque<nlohmann::json> pending_;
    mutable bool connected_{true};
};

} // namespace test
