#pragma once

#include <cstdint>
#include <string>
#include <thread>

namespace server {

struct ServerConfig {
    uint16_t port{9000};
    size_t thread_pool_size{0};
    uint32_t autosave_interval_sec{30};
    uint32_t cursor_broadcast_interval_ms{100};
    uint32_t idle_document_timeout_sec{300};
    uint32_t snapshot_revision_threshold{100};
    std::string data_dir{"./data"};
    uint32_t shutdown_timeout_sec{10};
    std::string log_level{"info"};

    size_t effective_thread_pool_size() const {
        if (thread_pool_size > 0) return thread_pool_size;
        auto hw = std::thread::hardware_concurrency();
        return hw > 0 ? hw : 4;
    }
};

ServerConfig load_config(const std::string& path);
ServerConfig default_config();

} // namespace server
