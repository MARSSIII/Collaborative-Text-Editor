#include "collab/thread_pool.h"

namespace collab {

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this](std::stop_token stop) {
            worker(stop);
        });
    }
}

ThreadPool::~ThreadPool() = default;

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard lock(queue_mutex_);
        tasks_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void ThreadPool::wait_idle() {
    std::unique_lock lock(idle_mutex_);
    idle_cv_.wait(lock, [this] {
        std::lock_guard q_lock(queue_mutex_);
        return tasks_.empty() && active_tasks_.load(std::memory_order_acquire) == 0;
    });
}

void ThreadPool::worker(std::stop_token stop) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(queue_mutex_);

            bool has_task = queue_cv_.wait(lock, stop, [this] {
                return !tasks_.empty();
            });

            if (!has_task) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        active_tasks_.fetch_add(1, std::memory_order_acq_rel);

        try {
            task();
        } catch (...) {
        }

        int remaining = active_tasks_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) {
            std::lock_guard lock(idle_mutex_);
            idle_cv_.notify_all();
        }
    }
}

}
