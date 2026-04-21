#include "collab/access_control.h"

namespace collab {

uint64_t AccessControl::make_key(uint32_t docId, uint32_t userId) {
    return (uint64_t(docId) << 32) | userId;
}

std::optional<Role> AccessControl::get_role_locked(uint32_t docId,
                                                   uint32_t userId) const {
    auto it = rights_.find(make_key(docId, userId));
    if (it == rights_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Role> AccessControl::get_role(uint32_t docId,
                                            uint32_t userId) const {
    std::lock_guard lock(mutex_);
    return get_role_locked(docId, userId);
}

void AccessControl::grant(uint32_t docId, uint32_t userId, Role role) {
    std::lock_guard lock(mutex_);
    rights_[make_key(docId, userId)] = role;
}

void AccessControl::revoke(uint32_t docId, uint32_t userId) {
    std::lock_guard lock(mutex_);
    rights_.erase(make_key(docId, userId));
}

void AccessControl::revoke_all_for_document(uint32_t docId) {
    std::lock_guard lock(mutex_);
    for (auto it = rights_.begin(); it != rights_.end(); ) {
        if (static_cast<uint32_t>(it->first >> 32) == docId) {
            it = rights_.erase(it);
        } else {
            ++it;
        }
    }
}

bool AccessControl::try_revoke(uint32_t docId, uint32_t requesterId,
                               uint32_t targetId) {
    std::lock_guard lock(mutex_);

    auto requesterIt = rights_.find(make_key(docId, requesterId));
    if (requesterIt == rights_.end() || requesterIt->second != Role::Owner) {
        return false;
    }

    if (requesterId == targetId) {
        return false;
    }

    return rights_.erase(make_key(docId, targetId)) > 0;
}

bool AccessControl::can_read(uint32_t docId, uint32_t userId) const {
    std::lock_guard lock(mutex_);
    return get_role_locked(docId, userId).has_value();
}

bool AccessControl::can_edit(uint32_t docId, uint32_t userId) const {
    std::lock_guard lock(mutex_);
    auto role = get_role_locked(docId, userId);
    return role.has_value() &&
           (role.value() == Role::Owner || role.value() == Role::Editor);
}

bool AccessControl::can_share(uint32_t docId, uint32_t userId) const {
    std::lock_guard lock(mutex_);
    auto role = get_role_locked(docId, userId);
    return role.has_value() && role.value() == Role::Owner;
}

bool AccessControl::can_delete(uint32_t docId, uint32_t userId) const {
    std::lock_guard lock(mutex_);
    auto role = get_role_locked(docId, userId);
    return role.has_value() && role.value() == Role::Owner;
}

std::vector<uint32_t>
AccessControl::list_documents_for_user(uint32_t userId) const {
    std::lock_guard lock(mutex_);
    std::vector<uint32_t> docs;
    for (const auto& [key, role] : rights_) {
        uint32_t storedUserId = static_cast<uint32_t>(key & 0xFFFFFFFF);
        if (storedUserId == userId) {
            uint32_t docId = static_cast<uint32_t>(key >> 32);
            docs.push_back(docId);
        }
    }
    return docs;
}

}
