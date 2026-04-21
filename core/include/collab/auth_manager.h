#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace collab {

struct AuthResult {
    bool success;
    uint32_t userId{0};
    std::string error;
};

class AuthManager {
public:
    AuthResult register_user(const std::string& username, const std::string& password);
    AuthResult login(const std::string& username, const std::string& password);
    std::string get_stored_hash(const std::string& username) const;
    std::optional<uint32_t> find_user_id(const std::string& username) const;

private:
    struct UserRecord {
        uint32_t id;
        std::string username;
        std::string password_hash;
        std::string salt;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserRecord> users_;
    uint32_t next_id_{1};
};

}
