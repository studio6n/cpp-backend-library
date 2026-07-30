#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "thread_configuration.hpp"

namespace impetus {
    namespace threads {
        /**
         * @brief
         * Global thread pool for discrete one-shot tasks.
         *
         * Meyers singleton — one instance for the lifetime of the application.
         * Initialised once via init(). Threads are created automatically with
         * names generated from the base name in ThreadConfiguration.
         *
         * Each submitted task is run exactly once by one idle thread.
         * After completion the thread returns to idle and waits for the next task.
         */
        class ThreadPool {
            public:
            /**
             * @brief
             * Returns the global ThreadPool instance.
             */
            [[nodiscard]]
            static ThreadPool& instance();

            /**
             * @brief
             * Copy construction is disabled.
             */
            ThreadPool(const ThreadPool&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            ThreadPool& operator=(const ThreadPool&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            ThreadPool(ThreadPool&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            ThreadPool& operator=(ThreadPool&&) = delete;

            /**
             * @brief
             * Stops all threads and joins them.
             */
            ~ThreadPool();

            public:
            /**
             * @brief
             * Creates thread_count threads and begins waiting for submitted tasks.
             * Must be called once before submit(). config.name is used as the base
             * name — threads are named "name-0", "name-1", etc., capped at 15 chars.
             *
             * @param config
             * Base configuration applied to all threads.
             *
             * @param thread_count
             * Number of threads to create. Defaults to 4.
             *
             * @return
             * True if initialisation succeeded. False if already initialised.
             */
            [[nodiscard]]
            bool init(ThreadConfiguration config, std::size_t thread_count = 4);

            /**
             * @brief
             * Requests all threads to stop after draining the task queue.
             */
            void stop() noexcept;

            /**
             * @brief
             * Submits a task to be run once by the next available idle thread.
             *
             * @param task
             * Callable to execute.
             *
             * @return
             * True if the task was queued successfully.
             */
            [[nodiscard]]
            bool submit(std::function<void()> task);

            /**
             * @brief
             * Returns the number of threads in the pool.
             */
            [[nodiscard]]
            std::size_t thread_count() const noexcept;

            /**
             * @brief
             * Returns the number of tasks currently waiting to be picked up.
             */
            [[nodiscard]]
            std::size_t pending_count() const;

            private:
            /**
             * @brief
             * Default constructor. Only accessible via instance().
             */
            ThreadPool() = default;

            private:
            /**
             * @brief
             * A single pooled OS thread. Defined in thread_pool.cpp.
             */
            struct PoolThread;

            private:
            /**
             * @brief
             * All pooled threads.
             */
            std::vector<std::unique_ptr<PoolThread>> threads_;

            /**
             * @brief
             * Tasks waiting to be picked up by an idle thread. Guarded by queue_mutex_.
             */
            std::queue<std::function<void()>> task_queue_;

            /**
             * @brief
             * Guards task_queue_.
             */
            mutable std::mutex queue_mutex_;

            /**
             * @brief
             * Wakes idle threads when a task is submitted or the pool is stopped.
             */
            std::condition_variable queue_cv_;
        };
    }
}
