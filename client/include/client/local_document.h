#pragma once

#include "collab/operation.h"

#include <cstdint>
#include <shared_mutex>
#include <string>

namespace collab_client {

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
