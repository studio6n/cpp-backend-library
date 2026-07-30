#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "event_poll_flag.hpp"

namespace impetus {
    namespace poll {
        class IEventPollListener;
        class EventPollSubscriber;
        class EventPollReactor;
    }
    namespace threads {
        class WorkerThread;
    }
}

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Owns and coordinates all epoll reactors and their worker threads.
         *
         * The main reactor runs on the calling thread and must be driven via run().
         * Worker reactors run on dedicated WorkerThread instances created by start().
         * All subscriber registration goes through the registry by named reactor handle.
         *
         * Meyers singleton — one instance for the lifetime of the application, so
         * any class can reach it via instance() without needing a reference passed
         * to it. Not copyable or movable.
         */
        class EventPollRegistry {
            public:
            /**
             * @brief
             * Returns the global EventPollRegistry instance.
             */
            [[nodiscard]]
            static EventPollRegistry& instance();

            /**
             * @brief
             * Copy construction is disabled.
             */
            EventPollRegistry(const EventPollRegistry&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            EventPollRegistry& operator=(const EventPollRegistry&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            EventPollRegistry(EventPollRegistry&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            EventPollRegistry& operator=(EventPollRegistry&&) = delete;

            /**
             * @brief
             * Calls stop_all(), stopping and removing every reactor.
             */
            ~EventPollRegistry();

            public:
            /**
             * @brief
             * Creates and starts the main reactor. Runs on the main thread — no WorkerThread.
             * Must be called once before any attach/detach/modify targeting the main reactor.
             *
             * @return
             * True if initialised successfully. False if already initialised.
             */
            [[nodiscard]]
            bool init();

            /**
             * @brief
             * Creates a reactor and a WorkerThread, then starts both immediately.
             * Returns false if the name already exists or if name is "main" (reserved — logs a warning).
             *
             * @param name
             * Unique name for this reactor and its worker thread.
             *
             * @return
             * True if created and started successfully.
             */
            [[nodiscard]]
            bool start(std::string_view name);

            /**
             * @brief
             * Stops the reactor and its WorkerThread, then removes the entry.
             * If name is "main", logs a warning and returns — the main reactor
             * cannot be stopped individually.
             *
             * @param name
             * Name of the reactor to stop.
             */
            void stop(std::string_view name) noexcept;

            /**
             * @brief
             * Processes events on the main reactor. The main reactor only works if
             * the main thread continuously calls this — it does not run on its own.
             * No-op if the main reactor has not been initialised or has been stopped.
             *
             * Hard requirement: must only ever be called from the same thread that
             * called init(). Enforced with assert() — calling from any other thread
             * is a programming error, not a runtime condition to handle gracefully.
             */
            void run() noexcept;

            /**
             * @brief
             * Stops and removes all reactors, including the main reactor.
             *
             * Hard requirement: must only ever be called from the same thread that
             * called init(). Enforced with assert() — calling from any other thread
             * is a programming error, not a runtime condition to handle gracefully.
             */
            void stop_all() noexcept;

            /**
             * @brief
             * Queues a subscriber for attachment on the named reactor's next iteration.
             *
             * @param reactor_name
             * Name of the target reactor.
             *
             * @param interest
             * Epoll interest flags.
             *
             * @param listener
             * Listener observing this subscriber's lifecycle.
             *
             * @param subscriber
             * Subscriber to attach.
             *
             * @return
             * True if queued successfully. False if reactor not found or attach fails.
             */
            [[nodiscard]]
            bool attach(std::string_view reactor_name,
                        EventPollFlag interest,
                        IEventPollListener* listener,
                        EventPollSubscriber* subscriber);

            /**
             * @brief
             * Queues a subscriber for detachment from the named reactor.
             *
             * @param reactor_name
             * Name of the target reactor.
             *
             * @param subscriber
             * Subscriber to detach.
             */
            void detach(std::string_view reactor_name, const EventPollSubscriber& subscriber);

            /**
             * @brief
             * Queues an interest flag modification on the named reactor.
             *
             * @param reactor_name
             * Name of the target reactor.
             *
             * @param fd
             * File descriptor of the attached subscriber.
             *
             * @param interest
             * New epoll interest flags.
             */
            void modify(std::string_view reactor_name, int fd, EventPollFlag interest);

            private:
            /**
             * @brief
             * Default constructor. Only accessible via instance(). Does not
             * create any reactors — call init() first.
             */
            EventPollRegistry() = default;

            private:
            /**
             * @brief
             * Pairs a reactor with its WorkerThread (nullptr for the main reactor).
             * Defined in event_poll_registry.cpp.
             */
            struct ReactorEntry;

            private:
            /**
             * @brief
             * Guards reactors_ and main_thread_id_.
             */
            mutable std::mutex mutex_;

            /**
             * @brief
             * Thread that called init(). Default-constructed (no thread) until then.
             * run() and stop_all() assert that they are called from this thread.
             */
            std::thread::id main_thread_id_;

            /**
             * @brief
             * All reactors, keyed by name. "main" is reserved for the reactor
             * created by init().
             */
            std::unordered_map<std::string, std::unique_ptr<ReactorEntry>> reactors_;
        };
    }
}
