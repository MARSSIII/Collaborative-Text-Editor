#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

namespace collab {

template <typename T, std::size_t MaxProducers = 64>
class MPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        Node* retired_next{nullptr};

        Node() : data{} {}
        explicit Node(T&& value) : data(std::move(value)) {}
    };

    struct alignas(64) HazardSlot {
        std::atomic<Node*> hazard{nullptr};
    };

    alignas(64) std::atomic<Node*> tail_;
    alignas(64) std::atomic<std::size_t> slots_used_{0};
    HazardSlot hazards_[MaxProducers];

    alignas(64) Node* head_;
    Node* retired_head_{nullptr};
    std::size_t retired_count_{0};

    static constexpr std::size_t RECLAIM_THRESHOLD = 2 * MaxProducers;

    using SlotCacheEntry = std::pair<void*, std::size_t>;
    static std::vector<SlotCacheEntry>& slot_cache_() {
        thread_local std::vector<SlotCacheEntry> cache;
        return cache;
    }

    std::size_t acquire_slot_() {
        auto& cache = slot_cache_();
        for (auto& [q, s] : cache) {
            if (q == this) return s;
        }
        std::size_t s = slots_used_.fetch_add(1, std::memory_order_relaxed);
        if (s >= MaxProducers) std::abort();
        cache.emplace_back(this, s);
        return s;
    }

    void try_reclaim_() noexcept {
        std::atomic_thread_fence(std::memory_order_seq_cst);

        Node* snapshot[MaxProducers];
        std::size_t n = 0;
        std::size_t used = slots_used_.load(std::memory_order_acquire);
        if (used > MaxProducers) used = MaxProducers;
        for (std::size_t i = 0; i < used; ++i) {
            Node* p = hazards_[i].hazard.load(std::memory_order_seq_cst);
            if (p) snapshot[n++] = p;
        }

        Node* survivors = nullptr;
        std::size_t kept = 0;
        Node* cur = retired_head_;
        while (cur) {
            Node* nx = cur->retired_next;
            bool guarded = false;
            for (std::size_t i = 0; i < n; ++i) {
                if (snapshot[i] == cur) { guarded = true; break; }
            }
            if (guarded) {
                cur->retired_next = survivors;
                survivors = cur;
                ++kept;
            } else {
                delete cur;
            }
            cur = nx;
        }
        retired_head_ = survivors;
        retired_count_ = kept;
    }

public:
    MPSCQueue() {
        auto* sentinel = new Node();
        tail_.store(sentinel, std::memory_order_relaxed);
        head_ = sentinel;
    }

    ~MPSCQueue() {
        while (try_dequeue()) {}
        Node* cur = retired_head_;
        while (cur) {
            Node* nx = cur->retired_next;
            delete cur;
            cur = nx;
        }
        delete head_;
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;
    MPSCQueue(MPSCQueue&&) = delete;
    MPSCQueue& operator=(MPSCQueue&&) = delete;

    void enqueue(T value) {
        auto* node = new Node(std::move(value));
        auto& hp = hazards_[acquire_slot_()].hazard;

        for (;;) {
            Node* t;
            for (;;) {
                t = tail_.load(std::memory_order_acquire);
                hp.store(t, std::memory_order_seq_cst);
                if (t == tail_.load(std::memory_order_acquire)) break;
            }

            Node* t_next = t->next.load(std::memory_order_acquire);

            if (t_next != nullptr) {
                tail_.compare_exchange_strong(
                    t, t_next,
                    std::memory_order_release,
                    std::memory_order_relaxed);
                continue;
            }

            Node* expected = nullptr;
            if (t->next.compare_exchange_strong(
                    expected, node,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                tail_.compare_exchange_strong(
                    t, node,
                    std::memory_order_release,
                    std::memory_order_relaxed);
                break;
            }
        }

        hp.store(nullptr, std::memory_order_release);
    }

    std::optional<T> try_dequeue() {
        Node* h = head_;
        Node* next = h->next.load(std::memory_order_acquire);
        if (!next) return std::nullopt;

        Node* t = tail_.load(std::memory_order_acquire);
        if (t == h) {
            tail_.compare_exchange_strong(
                t, next,
                std::memory_order_release,
                std::memory_order_relaxed);
        }

        T value = std::move(next->data);
        head_ = next;

        h->retired_next = retired_head_;
        retired_head_ = h;
        ++retired_count_;

        if (retired_count_ >= RECLAIM_THRESHOLD) try_reclaim_();
        return value;
    }
};

}  // namespace collab
