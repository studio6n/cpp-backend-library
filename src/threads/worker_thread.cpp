#include "worker_thread.hpp"

namespace impetus {
    namespace threads {
        WorkerThread::WorkerThread(ThreadConfiguration config)
            : BaseThread(std::move(config))
            , running_(false) {
        }

        bool WorkerThread::start(std::function<void()> loop_body) {
            if (running_.load()) {
                return false;
            }

            running_.store(true);
            thread_ = std::jthread([this, body = std::move(loop_body)](std::stop_token token) {
                thread_body(std::move(token), std::move(body));
            });

            return true;
        }

        void WorkerThread::stop() noexcept {
            thread_.request_stop();
        }

        bool WorkerThread::is_running() const noexcept {
            return running_.load();
        }

        void WorkerThread::thread_body(std::stop_token token, std::function<void()> loop_body) {
            apply_config();

            while (!token.stop_requested()) {
                loop_body();
            }

            running_.store(false);
        }
    }
}
