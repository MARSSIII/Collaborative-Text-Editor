#pragma once
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <optional>

namespace collab {

enum class Role { Owner, Editor, Viewer };

class AccessControl {
public:
    void grant(uint32_t docId, uint32_t userId, Role role);
    void revoke(uint32_t docId, uint32_t userId);

    bool try_revoke(uint32_t docId, uint32_t requesterId, uint32_t targetId);

    bool can_read(uint32_t docId, uint32_t userId) const;
    bool can_edit(uint32_t docId, uint32_t userId) const;
    bool can_share(uint32_t docId, uint32_t userId) const;
    bool can_delete(uint32_t docId, uint32_t userId) const;

    std::vector<uint32_t> list_documents_for_user(uint32_t userId) const;

    std::optional<Role> get_role(uint32_t docId, uint32_t userId) const;

private:
    static uint64_t make_key(uint32_t docId, uint32_t userId);
    std::optional<Role> get_role_locked(uint32_t docId, uint32_t userId) const;

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Role> rights_;
};

}
