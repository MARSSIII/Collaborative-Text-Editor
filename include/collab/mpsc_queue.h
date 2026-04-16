#pragma once
#include <atomic>
#include <optional>
#include <utility>

namespace collab {

template<typename T>
class MPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};

        Node() : data{}, next{nullptr} {}

        explicit Node(T&& value) : data(std::move(value)), next{nullptr} {}
    };

    alignas(64) std::atomic<Node*> head_;

    alignas(64) Node* tail_;

public:
    MPSCQueue() {
        auto* sentinel = new Node();
        head_.store(sentinel, std::memory_order_relaxed);
        tail_ = sentinel;
    }

    ~MPSCQueue() {
        while (try_dequeue()) {}
        delete tail_;
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;
    MPSCQueue(MPSCQueue&&) = delete;
    MPSCQueue& operator=(MPSCQueue&&) = delete;

    void enqueue(T value) {
        auto* node = new Node(std::move(value));
        auto* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    std::optional<T> try_dequeue() {
        Node* tail = tail_;
        Node* next = tail->next.load(std::memory_order_acquire);
        if (!next) return std::nullopt;

        T value = std::move(next->data);
        tail_ = next;
        delete tail;
        return value;
    }
};

}
