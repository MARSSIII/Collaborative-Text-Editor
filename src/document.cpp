#include "collab/document.h"
#include "collab/ot.h"

namespace collab {

Document::Document(uint32_t id, const std::string& title)
    : id_(id), title_(title) {}

OTResult Document::apply_with_ot(Operation op) {
    std::unique_lock lock(content_mutex_);

    if (revision_ > 0 && op.revision < oldest_revision_) {
        return {false, "revision_too_old", {}};
    }

    for (uint32_t i = op.revision; i < revision_; ++i) {
        uint32_t history_index = i - oldest_revision_;

        if (history_index < history_.size()) {
            op = transform(op, history_[history_index]).first;
        }
    }

    collab::apply(content_, op);

    history_.push_back(op);
    while (history_.size() > history_limit_) {
        history_.pop_front();

        oldest_revision_++;
    }

    revision_++;
    dirty_.store(true, std::memory_order_relaxed);

    return {true, "", op};
}

size_t Document::process_queue() {
    size_t count = 0;

    while (auto op = incoming_queue_.try_dequeue()) {
        apply_with_ot(std::move(*op));
        ++count;
    }

    return count;
}

std::string Document::get_content() const {
    std::shared_lock lock(content_mutex_);

    return content_;
}

uint32_t Document::revision() const {
    std::shared_lock lock(content_mutex_);

    return revision_;
}

void Document::set_content(const std::string& content) {
    std::unique_lock lock(content_mutex_);
    content_ = content;
}

void Document::load_content(const std::string& content) {
    std::unique_lock lock(content_mutex_);
    content_ = content;
    dirty_.store(false, std::memory_order_relaxed);
}

void Document::subscribe(uint32_t userId) {
    std::lock_guard lock(subscribers_mutex_);
    subscribers_.insert(userId);
}

void Document::unsubscribe(uint32_t userId) {
    std::lock_guard lock(subscribers_mutex_);
    subscribers_.erase(userId);
}

int Document::subscriber_count() const {
    std::lock_guard lock(subscribers_mutex_);

    return static_cast<int>(subscribers_.size());
}

bool Document::is_dirty() const {
    return dirty_.load(std::memory_order_relaxed);
}

void Document::mark_saved() {
    dirty_.store(false, std::memory_order_relaxed);
}

size_t Document::history_size() const {
    std::shared_lock lock(content_mutex_);

    return history_.size();
}

void Document::set_history_limit(uint32_t limit) {
    std::unique_lock lock(content_mutex_);
    history_limit_ = limit;
}

uint32_t Document::oldest_revision_in_history() const {
    std::shared_lock lock(content_mutex_);

    return oldest_revision_;
}

bool Document::needs_snapshot() const {
    std::shared_lock lock(content_mutex_);

    return snapshot_threshold_ > 0 &&
           revision_ > 0 &&
           revision_ % snapshot_threshold_ == 0 &&
           revision_ != last_snapshot_revision_;
}

void Document::mark_snapshot_created() {
    std::unique_lock lock(content_mutex_);
    last_snapshot_revision_ = revision_;
}

void Document::set_snapshot_threshold(uint32_t threshold) {
    std::unique_lock lock(content_mutex_);
    snapshot_threshold_ = threshold;
}

void Document::enqueue_operation(Operation op) {
    incoming_queue_.enqueue(std::move(op));
}

}
