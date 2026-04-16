#pragma once

#include "collab/operation.h"
#include "collab/mpsc_queue.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>

namespace collab {

struct OTResult {
    bool success;
    std::string error;
    Operation transformed_op;
};

class Document {
public:
    Document(uint32_t id, const std::string& title);

    OTResult apply_with_ot(Operation op);
    size_t process_queue();

    std::string get_content() const;
    uint32_t revision() const;
    std::string title() const noexcept { return title_; }
    uint32_t id() const noexcept { return id_; }

    void set_content(const std::string& content);
    void load_content(const std::string& content);

    void subscribe(uint32_t userId);
    void unsubscribe(uint32_t userId);
    int subscriber_count() const;

    bool is_dirty() const;
    void mark_saved();

    size_t history_size() const;
    void set_history_limit(uint32_t limit);
    uint32_t oldest_revision_in_history() const;

    bool needs_snapshot() const;
    void mark_snapshot_created();
    void set_snapshot_threshold(uint32_t threshold);

    void enqueue_operation(Operation op);

private:
    uint32_t id_;
    std::string title_;
    std::string content_;
    uint32_t revision_{0};

    std::deque<Operation> history_;
    uint32_t history_limit_{10000};
    uint32_t oldest_revision_{0};

    MPSCQueue<Operation> incoming_queue_;
    std::set<uint32_t> subscribers_;
    mutable std::mutex subscribers_mutex_;

    std::atomic<bool> dirty_{false};
    mutable std::shared_mutex content_mutex_;

    uint32_t snapshot_threshold_{100};
    uint32_t last_snapshot_revision_{0};
};

}
