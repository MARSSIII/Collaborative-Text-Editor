#include <gtest/gtest.h>
#include "collab/thread_pool.h"

#include <atomic>

using namespace collab;
#include <chrono>
#include <thread>

TEST(ThreadPool, ExecutesTasks) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(4);

        for (int i = 0; i < 100; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool.wait_idle();
    }

    EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPool, ConcurrentExecution) {
    std::atomic<int> active{0};
    std::atomic<int> max_active{0};

    {
        ThreadPool pool(4);

        for (int i = 0; i < 20; ++i) {
            pool.submit([&]() {
                int current = active.fetch_add(1, std::memory_order_relaxed) + 1;

                int prev_max = max_active.load(std::memory_order_relaxed);
                while (current > prev_max &&
                       !max_active.compare_exchange_weak(prev_max, current,
                                                         std::memory_order_relaxed)) {
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                active.fetch_sub(1, std::memory_order_relaxed);
            });
        }

        pool.wait_idle();
    }

    EXPECT_GT(max_active.load(), 1)
        << "Expected concurrent execution, but max active tasks was "
        << max_active.load();
}

TEST(ThreadPool, GracefulShutdown) {
    std::atomic<int> completed{0};

    {
        ThreadPool pool(2);

        for (int i = 0; i < 10; ++i) {
            pool.submit([&completed]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                completed.fetch_add(1, std::memory_order_relaxed);
            });
        }
    }

    EXPECT_EQ(completed.load(), 10)
        << "Not all tasks completed before pool shutdown";
}

TEST(ThreadPool, WaitIdleOnEmptyPool) {
    ThreadPool pool(4);
    pool.wait_idle();
}

TEST(ThreadPool, SingleThreadManyTasks) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(1);
        for (int i = 0; i < 200; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        pool.wait_idle();
    }
    EXPECT_EQ(counter.load(), 200);
}

TEST(ThreadPool, TaskThrowsException) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);

        pool.submit([]() { throw std::runtime_error("test"); });

        for (int i = 0; i < 10; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool.wait_idle();
    }
    EXPECT_EQ(counter.load(), 10);
}
