#pragma once

#include "collab/operation.h"

#include <cstdint>
#include <shared_mutex>
#include <string>

namespace collab_client {

// Stores the authoritative UTF-8 snapshot that the client considers current.
// GUI-thread apply()s are serialized by QTextDocument events; LocalAutosave
// (Phase 7) will read via shared_lock. Kept thread-safe from day one to avoid
// retrofitting later.
class LocalDocument {
public:
    LocalDocument() = default;
    LocalDocument(std::string initial_content, uint32_t initial_revision);

    void reset(std::string content, uint32_t revision);

    void apply(const collab::Operation& op);

    std::string snapshot() const;
    uint32_t revision() const;
    void set_revision(uint32_t rev);

private:
    mutable std::shared_mutex mtx_;
    std::string content_;
    uint32_t revision_ = 0;
};

} // namespace collab_client
