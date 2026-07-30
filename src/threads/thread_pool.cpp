#include "thread_pool.hpp"

#include <cassert>
#include <string>
#include <thread>

#include "base_thread.hpp"

namespace impetus {
    namespace threads {
        ThreadPool& ThreadPool::instance() {
            static ThreadPool pool;
            return pool;
        }

        /**
         * @brief
         * A single pooled OS thread. Spawns immediately on construction and
         * waits on the shared task queue until stopped.
         */
        struct ThreadPool::PoolThread : public BaseThread {
            /**
             * @brief
             * Constructs a pool thread and spawns it immediately.
             * The thread applies its config then waits for tasks from the shared queue.
             *
             * @param config
             * Configuration for this thread's name, affinity, and scheduling.
             *
             * @param task_queue
             * Shared task queue owned by the pool.
             *
             * @param queue_mutex
             * Mutex guarding task_queue.
             *
             * @param queue_cv
             * Condition variable used to wake idle threads.
             */
            PoolThread(ThreadConfiguration config,
                       std::queue<std::function<void()>>& task_queue,
                       std::mutex& queue_mutex,
                       std::condition_variable& queue_cv)
                : BaseThread(std::move(config))
                , thread_([this, &task_queue, &queue_mutex, &queue_cv](std::stop_token token) {
                    apply_config();

                    while (true) {
                        std::unique_lock lock(queue_mutex);
                        queue_cv.wait(lock, [&task_queue, &token] {
                            return !task_queue.empty() || token.stop_requested();
                        });

                        if (token.stop_requested() && task_queue.empty()) {
                            break;
                        }

                        auto task = std::move(task_queue.front());
                        task_queue.pop();
                        lock.unlock();

                        // Run the task exactly once, then return to waiting.
                        task();
                    }
                }) {
            }

            /**
             * @brief
             * The underlying OS thread running the task-waiting loop.
             */
            std::jthread thread_;
        };

        ThreadPool::~ThreadPool() = default;

        bool ThreadPool::init(ThreadConfiguration config, std::size_t thread_count) {
            if (!threads_.empty()) {
                return false;
            }

            assert(thread_count > 0 && "thread_count must be greater than zero");

            const std::string base_name = config.name;

            for (std::size_t i = 0; i < thread_count; ++i) {
                ThreadConfiguration thread_config = config;

                // Generate per-thread name: "base-0", "base-1", etc., capped at 15 chars.
                std::string name = base_name + "-" + std::to_string(i);
                if (name.size() > 15) {
                    name.resize(15);
                }

                thread_config.name = std::move(name);

                threads_.push_back(std::make_unique<PoolThread>(
                    std::move(thread_config), task_queue_, queue_mutex_, queue_cv_));
            }

            return true;
        }

        void ThreadPool::stop() noexcept {
            for (auto& thread : threads_) {
                thread->thread_.request_stop();
            }

            queue_cv_.notify_all();
        }

        bool ThreadPool::submit(std::function<void()> task) {
            {
                std::lock_guard lock(queue_mutex_);
                task_queue_.push(std::move(task));
            }
            queue_cv_.notify_one();
            return true;
        }

        std::size_t ThreadPool::thread_count() const noexcept {
            return threads_.size();
        }

        std::size_t ThreadPool::pending_count() const {
            std::lock_guard lock(queue_mutex_);
            return task_queue_.size();
        }
    }
}
