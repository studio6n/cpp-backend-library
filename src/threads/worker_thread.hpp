#pragma once

#include <atomic>
#include <functional>
#include <thread>

#include "base_thread.hpp"

namespace impetus {
    namespace threads {
        /**
         * @brief
         * Owns a single OS thread that drives a continuous loop on a dedicated thread.
         *
         * Intended for long-running work such as event loops and reactor dispatch.
         * The provided loop body is called repeatedly until stop() is requested.
         * Not copyable or movable.
         */
        class WorkerThread : public BaseThread {
            public:
            /**
             * @brief
             * Default construction is disabled — a ThreadConfiguration is a hard requirement.
             */
            WorkerThread() = delete;

            /**
             * @brief
             * Constructs the worker with the given configuration. Does not
             * start the thread — call start() to begin running.
             *
             * @param config
             * Configuration to apply once the thread is running.
             */
            explicit WorkerThread(ThreadConfiguration config);

            /**
             * @brief
             * Copy construction is disabled.
             */
            WorkerThread(const WorkerThread&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            WorkerThread& operator=(const WorkerThread&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            WorkerThread(WorkerThread&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            WorkerThread& operator=(WorkerThread&&) = delete;

            /**
             * @brief
             * Stops and joins the thread if still running.
             */
            ~WorkerThread() = default;

            public:
            /**
             * @brief
             * Spawns the OS thread and begins calling loop_body repeatedly.
             *
             * @param loop_body
             * Callable invoked on each loop iteration until stop is requested.
             *
             * @return
             * True if the thread was started successfully, false if already running.
             */
            [[nodiscard]]
            bool start(std::function<void()> loop_body);

            /**
             * @brief
             * Requests the thread to stop after the current loop_body call returns.
             * The thread is joined automatically when this object is destroyed.
             */
            void stop() noexcept;

            /**
             * @brief
             * Returns true if the thread is currently running.
             */
            [[nodiscard]]
            bool is_running() const noexcept;

            private:
            /**
             * @brief
             * Entry point of the OS thread. Applies config then loops calling loop_body.
             *
             * @param token
             * Stop token checked between iterations.
             *
             * @param loop_body
             * Callable invoked repeatedly until token signals stop.
             */
            void thread_body(std::stop_token token, std::function<void()> loop_body);

            private:
            /**
             * @brief
             * True while the thread is running.
             */
            std::atomic<bool> running_;

            private:
            /**
             * @brief
             * The underlying OS thread.
             */
            std::jthread thread_;
        };
    }
}
