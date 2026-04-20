#include "client/local_document.h"

#include "collab/ot.h"

namespace collab_client {

LocalDocument::LocalDocument(std::string initial_content, uint32_t initial_revision)
    : content_(std::move(initial_content)), revision_(initial_revision) {}

void LocalDocument::reset(std::string content, uint32_t revision) {
    std::unique_lock lock(mtx_);
    content_ = std::move(content);
    revision_ = revision;
}

void LocalDocument::apply(const collab::Operation& op) {
    std::unique_lock lock(mtx_);
    collab::apply(content_, op);
}

std::string LocalDocument::snapshot() const {
    std::shared_lock lock(mtx_);
    return content_;
}

uint32_t LocalDocument::revision() const {
    std::shared_lock lock(mtx_);
    return revision_;
}

void LocalDocument::set_revision(uint32_t rev) {
    std::unique_lock lock(mtx_);
    revision_ = rev;
}

} // namespace collab_client
