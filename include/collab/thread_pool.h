#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace collab {

class ThreadPool {
public:
  explicit ThreadPool(size_t num_threads);

  ~ThreadPool();

  void submit(std::function<void()> task);

  void wait_idle();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

private:
  void worker(std::stop_token stop);

  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable_any queue_cv_;

  std::atomic<int> active_tasks_{0};
  std::mutex idle_mutex_;
  std::condition_variable idle_cv_;

  std::vector<std::jthread> workers_;
};

}
