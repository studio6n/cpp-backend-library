#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "event_poll_flag.hpp"
#include "event_poll_subscriber.hpp"

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Drives an epoll instance, dispatching I/O events to registered subscribers.
         *
         * Thread-safe: attach(), detach(), and modify() may be called from any thread.
         * run() must be called from the reactor's dedicated thread.
         * Not copyable or movable — owns the epoll file descriptor.
         *
         * Holds subscribers by weak_ptr. Closes every file descriptor it accepted:
         * on detach, after removing it, so the number cannot be reused while an
         * entry still refers to it; or when the reactor is destroyed.
         */
        class EventPollReactor final {
            public:
            /**
             * @brief
             * Default construction is disabled — a name suffix is a hard requirement.
             */
            EventPollReactor() = delete;

            /**
             * @brief
             * Constructs a reactor with a name suffix.
             * Subscription capacity defaults to K_EPOLL_SUBSCRIPTION_SIZE.
             *
             * @param suffix
             * Suffix appended to the reactor name.
             */
            explicit EventPollReactor(std::string_view suffix);

            /**
             * @brief
             * Constructs a reactor with a name suffix and subscription capacity.
             *
             * @param suffix
             * Suffix appended to the reactor name.
             *
             * @param max_subscriptions
             * Maximum number of concurrent subscriptions.
             */
            explicit EventPollReactor(std::string_view suffix, std::size_t max_subscriptions);

            /**
             * @brief
             * Closes every file descriptor still attached or queued, then the epoll
             * file descriptor.
             */
            ~EventPollReactor();

            /**
             * @brief
             * Copy construction is disabled.
             */
            EventPollReactor(const EventPollReactor&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            EventPollReactor& operator=(const EventPollReactor&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            EventPollReactor(EventPollReactor&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            EventPollReactor& operator=(EventPollReactor&&) = delete;

            public:
            /**
             * @brief
             * Creates the epoll file descriptor and transitions the reactor to running state.
             *
             * @return
             * True if the reactor started successfully, false otherwise.
             */
            [[nodiscard]]
            bool start();

            /**
             * @brief
             * Flushes all pending attach, modify, and detach requests in order,
             * then waits for epoll events. Must be called from the reactor's dedicated thread.
             */
            void run();

            /**
             * @brief
             * Signals the reactor to stop on the next iteration.
             */
            void stop() noexcept;

            /**
             * @brief
             * Queues a subscriber for attachment on the next reactor iteration. Thread-safe.
             *
             * @param interest
             * Epoll interest flags for the subscriber.
             *
             * @param subscriber
             * Subscriber to attach. Must be owned by a std::shared_ptr.
             *
             * @return
             * True if queued successfully; from then on this reactor closes the fd.
             * False if the subscriber is gone, its fd is invalid, the reactor is at
             * capacity, or the fd is already queued.
             */
            [[nodiscard]]
            bool attach(EventPollFlag interest, std::weak_ptr<EventPollSubscriber> subscriber);

            /**
             * @brief
             * Hands the subscriber's file descriptor back for detachment on the next
             * reactor iteration, which removes it and then closes it. Thread-safe.
             *
             * @param subscriber
             * Subscriber to detach.
             */
            void detach(const EventPollSubscriber& subscriber);

            /**
             * @brief
             * Queues an interest flag modification for the next reactor iteration. Thread-safe.
             *
             * @param fd
             * File descriptor of the attached subscriber.
             *
             * @param interest
             * New epoll interest flags.
             */
            void modify(int fd, EventPollFlag interest);

            /**
             * @brief
             * Returns true if the reactor is in running state.
             */
            [[nodiscard]]
            bool in_running_state() const noexcept {
                return reactor_state_.load() == ReactorState::RUNNING;
            }

            /**
             * @brief
             * Returns true if the reactor failed to create its epoll fd.
             */
            [[nodiscard]]
            bool in_error_state() const noexcept {
                return reactor_state_.load() == ReactorState::ERROR_EPOLL_FD;
            }

            private:
            /**
             * @brief
             * Lifecycle states of the reactor.
             */
            enum class ReactorState : std::uint8_t {
                NOT_STARTED = 0,
                RUNNING,
                STOPPING,
                STOPPED,
                ERROR_EPOLL_FD
            };

            /**
             * @brief
             * Groups the file descriptor, epoll interest flag and subscriber into a
             * single tracked entry.
             */
            struct Subscription {
                /**
                 * @brief
                 * File descriptor, recorded at attach() so the reactor never has to
                 * ask a subscriber that may be gone.
                 */
                int fd = EventPollSubscriber::K_INVALID_FD;

                /**
                 * @brief
                 * Epoll interest flags for this subscription.
                 */
                EventPollFlag flag = EventPollFlag::NONE;

                /**
                 * @brief
                 * The subscriber itself. Not owned — locked only for the length of
                 * each callback.
                 */
                std::weak_ptr<EventPollSubscriber> subscriber;
            };

            /**
             * @brief
             * Carries a pending flag modification for an attached subscriber.
             */
            struct PendingModify {
                /**
                 * @brief
                 * File descriptor of the attached subscriber to modify.
                 */
                int fd;

                /**
                 * @brief
                 * New epoll interest flags to apply.
                 */
                EventPollFlag flag;
            };

            private:
            /**
             * @brief
             * Blocks on epoll_wait and dispatches any returned events.
             */
            void wait_for_events();

            /**
             * @brief
             * Routes each epoll event to the correct subscriber callback.
             *
             * @param events
             * Array of epoll events returned by epoll_wait.
             *
             * @param count
             * Number of valid entries in the array.
             */
            void dispatch_events(epoll_event* events, int count);

            /**
             * @brief
             * Promotes all queued attach requests into active subscriptions.
             *
             * @param local
             * Swapped-out pending attach vector, processed without holding the mutex.
             */
            void flush_attach_requests(std::vector<Subscription>& local);

            /**
             * @brief
             * Applies all queued modify requests to active subscriptions.
             *
             * @param local
             * Swapped-out pending modify vector, processed without holding the mutex.
             */
            void flush_modify_requests(std::vector<PendingModify>& local);

            /**
             * @brief
             * Processes all queued detach requests, removing each file descriptor
             * from epoll and the subscription table, closing it, and queueing its
             * on_detached() callback.
             *
             * @param local
             * Swapped-out pending detach vector, processed without holding the mutex.
             */
            void flush_detach_requests(std::vector<int>& local);

            /**
             * @brief
             * Fires on_detached() for all subscriptions that completed detachment.
             */
            void flush_detached_callbacks();

            /**
             * @brief
             * Builds the reactor name from the given suffix.
             *
             * @param suffix
             * Suffix to append to the name prefix.
             *
             * @return
             * True if the name was set successfully.
             */
            bool set_reactor_name(std::string_view suffix);

            /**
             * @brief
             * Registers a file descriptor with epoll using EPOLL_CTL_ADD.
             */
            [[nodiscard]]
            bool epoll_add(int fd, std::uint32_t events) noexcept;

            /**
             * @brief
             * Updates a registered file descriptor's interest mask using EPOLL_CTL_MOD.
             */
            [[nodiscard]]
            bool epoll_mod(int fd, std::uint32_t events) noexcept;

            /**
             * @brief
             * Removes a file descriptor from epoll using EPOLL_CTL_DEL.
             */
            void epoll_del(int fd) noexcept;

            private:
            /**
             * @brief
             * Human-readable name for this reactor instance.
             */
            std::string reactor_name_;

            /**
             * @brief
             * Active subscriptions keyed by file descriptor. Reactor thread only.
             */
            std::unordered_map<int, Subscription> subscriptions_;

            /**
             * @brief
             * Subscriptions queued for attachment. Protected by pending_mutex_.
             */
            std::vector<Subscription> pending_attach_subscriptions_;

            /**
             * @brief
             * File descriptors handed back for detachment and closing. Protected by
             * pending_mutex_.
             */
            std::vector<int> pending_detach_fds_;

            /**
             * @brief
             * Pending flag modifications. Protected by pending_mutex_.
             */
            std::vector<PendingModify> pending_modify_entries_;

            /**
             * @brief
             * Subscriptions waiting for their on_detached() callback to fire. Reactor thread only.
             */
            std::vector<Subscription> pending_detached_subscriptions_;

            /**
             * @brief
             * Guards all three pending queues.
             */
            std::mutex pending_mutex_;

            /**
             * @brief
             * Current lifecycle state of the reactor.
             */
            std::atomic<ReactorState> reactor_state_;

            /**
             * @brief
             * Number of active subscriptions. Updated by the reactor thread.
             * Read by attach() under pending_mutex_ for capacity enforcement.
             */
            std::atomic<std::size_t> subscription_count_;

            /**
             * @brief
             * Maximum number of concurrent subscriptions allowed.
             */
            std::size_t max_subscriptions_;

            /**
             * @brief
             * Epoll file descriptor owned by this reactor.
             */
            int epoll_fd_;
        };
    }
}
