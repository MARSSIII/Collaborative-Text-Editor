#include "server/server_config.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace server {

ServerConfig default_config() {
    return {};
}

ServerConfig load_config(const std::string& path) {
    ServerConfig config;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Config file not found: " << path
                  << ", using defaults\n";
        return config;
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Failed to parse config: " << e.what()
                  << ", using defaults\n";
        return config;
    }

    auto get = [&]<typename T>(const char* key, T& field) {
        if (j.contains(key)) {
            try {
                field = j[key].get<T>();
            } catch (...) {
                std::cerr << "Invalid value for " << key << ", using default\n";
            }
        }
    };

    get("port", config.port);
    get("thread_pool_size", config.thread_pool_size);
    get("autosave_interval_sec", config.autosave_interval_sec);
    get("cursor_broadcast_interval_ms", config.cursor_broadcast_interval_ms);
    get("idle_document_timeout_sec", config.idle_document_timeout_sec);
    get("snapshot_revision_threshold", config.snapshot_revision_threshold);
    get("data_dir", config.data_dir);
    get("shutdown_timeout_sec", config.shutdown_timeout_sec);
    get("log_level", config.log_level);
    get("log_console", config.log_console);

    return config;
}

}
