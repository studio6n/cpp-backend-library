#pragma once

#include "event_poll_listener.hpp"
#include "shared/shared_event_poll_helper.hpp"
#include "shared/shared_kernel_helper.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Reactor for dispatching epoll events to subscribers.
         *
         * This class manages subscriber registration, lifecycle callbacks,
         * and event dispatch for a single epoll instance.
         */
        class EventPollReactor final {
            public:
            /**
             * @brief
             * Reactor must be constructed with a name suffix.
             */
            EventPollReactor() = delete;

            /**
             * @brief
             * Constructs a reactor with a name suffix.
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
             * @param subscription_size
             * Reserved capacity for subscription containers.
             */
            explicit EventPollReactor(std::string_view suffix, std::size_t subscription_size);
            ~EventPollReactor();

            EventPollReactor(const EventPollReactor&) = delete;
            EventPollReactor& operator=(const EventPollReactor&) = delete;
            EventPollReactor(EventPollReactor&&) = delete;
            EventPollReactor& operator=(EventPollReactor&&) = delete;

            /**
             * @brief
             * Creates the epoll file descriptor and transitions the reactor to running state.
             *
             * @return
             * TRUE if the reactor starts successfully, FALSE otherwise.
             */
            [[nodiscard]]
            bool start();

            /**
             * @brief
             * Runs one reactor iteration.
             *
             * This flushes pending attachments, flushes pending detach callbacks,
             * and waits for epoll events when the reactor is running.
             */
            void run();

            /**
             * @brief
             * Requests the reactor to stop.
             */
            void stop() noexcept;

            /**
             * @brief
             * Attaches a subscriber.
             * @param interest
             * Under which epoll flag the subscriber needs to be in.
             * @param listener
             * The listener that manages the lifecycle of the subscriber.
             * @param subscriber
             * The subscriber that you wish to attach to.
             */
            [[nodiscard]]
            bool attach(shared::EventPollFlag interest,
                        EventPollListener* listener,
                        EventPollSubscriber* subscriber);

            /**
             * @brief
             * Queues a subscriber for attachment.
             *
             * @param interest
             * Epoll interest flags for the subscriber.
             * @param listener
             * Listener that observes this subscriber's lifecycle.
             * @param subscriber
             * Subscriber to be attached.
             *
             * @return
             * TRUE if the subscriber is queued successfully, FALSE otherwise.
             */
            [[nodiscard]]
            bool modify(int fd, shared::EventPollFlag interest);

            /**
             * @brief
             * Queues a subscriber for detachment by subscriber reference.
             *
             * @param subscriber
             * Subscriber to be detached.
             *
             * @return
             * TRUE if detachment succeeds, FALSE otherwise.
             */
            [[nodiscard]]
            bool detach(const EventPollSubscriber& subscriber) {
                return detach_subscriber(subscriber.fd());
            }

            /**
             * @brief
             * Checks whether the reactor is running.
             *
             * @return
             * TRUE if the reactor is in running state, FALSE otherwise.
             */
            inline bool in_running_state() const noexcept {
                return reactor_state_.load() == ReactorState::RUNNING;
            }

            /**
             * @brief
             * Checks whether the reactor is in epoll error state.
             *
             * @return
             * TRUE if the reactor failed to create its epoll fd, FALSE otherwise.
             */
            inline bool in_error_state() const noexcept {
                return reactor_state_.load() == ReactorState::ERROR_EPOLL_FD;
            }

            private:
            /**
             * @brief
             * States used by the reactor lifecycle.
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
             * Pairing between epoll interest, listener, and subscriber.
             */
            struct Subscription {
                impetus::shared::EventPollFlag flag = impetus::shared::EventPollFlag::NONE;
                EventPollListener* listener = nullptr;
                EventPollSubscriber* subscriber = nullptr;
            };

            /**
             * @brief
             * Waits for epoll events and dispatches them.
             */
            void wait_for_events();

            /**
             * @brief
             * Dispatches epoll events to their corresponding subscribers.
             *
             * @param events
             * Array of epoll events returned by epoll_wait().
             * @param count
             * Number of valid entries in the array.
             */
            void dispatch_events(epoll_event* events, int count);

            /**
             * @brief
             * Detaches a subscriber by file descriptor.
             *
             * @param fd
             * Subscriber file descriptor.
             *
             * @return
             * TRUE if the subscriber is detached, FALSE otherwise.
             */
            bool detach_subscriber(int fd);

            /**
             * @brief
             * Flushes all subscribers queued for attachment.
             */
            void flush_attaching_subscriptions();

            /**
             * @brief
             * Flushes all subscribers queued for detach-complete callbacks.
             */
            void flush_detaching_subscriptions();

            /**
             * @brief
             * Sets the reactor name from a suffix.
             *
             * @param suffix
             * Reactor suffix string.
             *
             * @return
             * TRUE if the name is assigned successfully, FALSE otherwise.
             */
            bool set_reactor_name(std::string_view suffix);

            /**
             * @brief
             * Adds a file descriptor to epoll.
             *
             * @param fd
             * File descriptor to add.
             * @param events
             * Native epoll events.
             *
             * @return
             * TRUE if epoll_ctl succeeds, FALSE otherwise.
             */
            [[nodiscard]]
            bool epoll_add(int fd, std::uint32_t events) noexcept;

            /**
             * @brief
             * Modifies a file descriptor already registered in epoll.
             *
             * @param fd
             * File descriptor to modify.
             * @param events
             * Native epoll events.
             *
             * @return
             * TRUE if epoll_ctl succeeds, FALSE otherwise.
             */
            [[nodiscard]]
            bool epoll_mod(int fd, std::uint32_t events) noexcept;

            /**
             * @brief
             * Removes a file descriptor from epoll.
             *
             * @param fd
             * File descriptor to remove.
             */
            void epoll_del(int fd) noexcept;

            private:
            /**
             * @brief
             * Reactor display name.
             */
            char reactor_name_[16];

            /**
             * @brief
             * Active subscriptions keyed by subscriber file descriptor.
             */
            std::unordered_map<int, Subscription> subscriptions_;

            /**
             * @brief
             * Subscriptions waiting to be attached.
             */
            std::vector<Subscription> pending_attach_subscriptions_;

            /**
             * @brief
             * Subscriptions waiting for detach-complete callbacks.
             */
            std::vector<Subscription> pending_detached_subscriptions_;

            /**
             * @brief
             * Current lifecycle state of the reactor.
             */
            std::atomic<ReactorState> reactor_state_;

            /**
             * @brief
             * Epoll file descriptor owned by this reactor.
             */
            int epoll_fd_;
        };
    }
}