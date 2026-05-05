#include <gtest/gtest.h>
#include "collab/thread_pool.h"

#include <atomic>
#include <latch>
#include <stdexcept>

using namespace collab;

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
    constexpr int POOL_SIZE = 4;
    constexpr int NUM_TASKS = 20;

    std::atomic<int> active{0};
    std::atomic<int> max_active{0};
    std::atomic<int> arrivals{0};
    std::latch first_wave{POOL_SIZE};

    {
        ThreadPool pool(POOL_SIZE);

        for (int i = 0; i < NUM_TASKS; ++i) {
            pool.submit([&]() {
                int current = active.fetch_add(1, std::memory_order_relaxed) + 1;
                int prev_max = max_active.load(std::memory_order_relaxed);
                while (current > prev_max &&
                       !max_active.compare_exchange_weak(prev_max, current,
                                                         std::memory_order_relaxed)) {
                }

                if (arrivals.fetch_add(1, std::memory_order_relaxed) < POOL_SIZE) {
                    first_wave.arrive_and_wait();
                }

                active.fetch_sub(1, std::memory_order_relaxed);
            });
        }

        pool.wait_idle();
    }

    EXPECT_EQ(max_active.load(), POOL_SIZE)
        << "Expected all " << POOL_SIZE << " pool threads to run concurrently, got "
        << max_active.load();
}

TEST(ThreadPool, GracefulShutdown) {
    std::atomic<int> completed{0};
    std::latch release_tasks{1};

    {
        ThreadPool pool(2);

        for (int i = 0; i < 10; ++i) {
            pool.submit([&]() {
                release_tasks.wait();
                completed.fetch_add(1, std::memory_order_relaxed);
            });
        }

        release_tasks.count_down();
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

TEST(ThreadPool, ExceptionDoesNotKillWorkers) {
    constexpr int POOL_SIZE = 4;

    std::atomic<int> max_active{0};
    std::atomic<int> active{0};
    std::atomic<int> arrivals{0};
    std::latch wave{POOL_SIZE};

    {
        ThreadPool pool(POOL_SIZE);

        for (int i = 0; i < POOL_SIZE; ++i) {
            pool.submit([] { throw std::runtime_error("boom"); });
        }
        pool.wait_idle();

        for (int i = 0; i < POOL_SIZE * 2; ++i) {
            pool.submit([&] {
                int current = active.fetch_add(1, std::memory_order_relaxed) + 1;
                int prev_max = max_active.load(std::memory_order_relaxed);
                while (current > prev_max &&
                       !max_active.compare_exchange_weak(prev_max, current,
                                                         std::memory_order_relaxed)) {
                }

                if (arrivals.fetch_add(1, std::memory_order_relaxed) < POOL_SIZE) {
                    wave.arrive_and_wait();
                }

                active.fetch_sub(1, std::memory_order_relaxed);
            });
        }

        pool.wait_idle();
    }

    EXPECT_EQ(max_active.load(), POOL_SIZE)
        << "Pool lost workers after task exceptions: only "
        << max_active.load() << "/" << POOL_SIZE << " ran concurrently";
}
