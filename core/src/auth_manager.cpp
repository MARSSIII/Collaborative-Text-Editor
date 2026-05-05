#include "collab/auth_manager.h"
#include "collab/hash_utils.h"

namespace collab {

AuthResult AuthManager::register_user(const std::string& username,
                                      const std::string& password) {
    if (username.empty()) {
        return {false, 0, "empty_username"};
    }

    if (password.empty()) {
        return {false, 0, "empty_password"};
    }

    std::lock_guard lock(mutex_);

    if (users_.contains(username)) {
        return {false, 0, "duplicate_username"};
    }

    std::string salt = generate_salt();
    std::string hash = hash_password(password, salt);
    uint32_t id = next_id_++;

    users_.emplace(username, UserRecord{id, username, hash, salt});
    return {true, id, ""};
}

AuthResult AuthManager::login(const std::string& username,
                              const std::string& password) {
    std::lock_guard lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return {false, 0, "invalid_credentials"};
    }

    const auto& record = it->second;
    if (!verify_password(password, record.salt, record.password_hash)) {
        return {false, 0, "invalid_credentials"};
    }

    return {true, record.id, ""};
}

std::string AuthManager::get_stored_hash(const std::string& username) const {
    std::lock_guard lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return "";
    }

    return it->second.password_hash;
}

std::optional<uint32_t> AuthManager::find_user_id(const std::string& username) const {
    std::lock_guard lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return std::nullopt;
    }

    return it->second.id;
}

}
