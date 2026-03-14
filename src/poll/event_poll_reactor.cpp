#include "event_poll_reactor.hpp"

#include "shared/shared_constants.hpp"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#ifndef EPOLL_MAX_EVENTS
#define EPOLL_MAX_EVENTS 100
#endif

#ifndef EPOLL_TIMEOUT_MS
#define EPOLL_TIMEOUT_MS 100
#endif

#ifndef EPOLL_SUBSCRIPTION_SIZE
#define EPOLL_SUBSCRIPTION_SIZE 50
#endif

#define REACTOR_NAME_LIMIT 4

static constexpr std::string_view S_REACTOR_NAME_PREFIX = "ep-reactor-";

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Constructs a reactor with a name suffix.
         *
         * @param suffix
         * Suffix appended to the reactor name.
         */
        EventPollReactor::EventPollReactor(std::string_view suffix)
            : reactor_state_(ReactorState::NOT_STARTED)
            , epoll_fd_(-1) {
            set_reactor_name(suffix);
            std::size_t size = EPOLL_SUBSCRIPTION_SIZE;
            subscriptions_.reserve(size);
            pending_attach_subscriptions_.reserve(size);
            pending_detached_subscriptions_.reserve(size);
        }

        /**
         * @brief
         * Constructs a reactor with a name suffix and subscription capacity.
         *
         * @param suffix
         * Suffix appended to the reactor name.
         * @param subscription_size
         * Reserved capacity for subscription containers.
         */
        EventPollReactor::EventPollReactor(std::string_view suffix, std::size_t subscription_size)
            : reactor_state_(ReactorState::NOT_STARTED)
            , epoll_fd_(-1) {
            set_reactor_name(suffix);
            std::size_t new_subscription_size = subscription_size;
            if (new_subscription_size <= shared::K_SIZE_T_NEUTRAL) {
                new_subscription_size = EPOLL_SUBSCRIPTION_SIZE;
            }

            subscriptions_.reserve(new_subscription_size);
            pending_attach_subscriptions_.reserve(new_subscription_size);
            pending_detached_subscriptions_.reserve(new_subscription_size);
        }

        /**
         * @brief
         * Creates the epoll file descriptor and transitions the reactor to running state.
         *
         * @return
         * TRUE if the reactor starts successfully, FALSE otherwise.
         */
        bool EventPollReactor::start() {
            epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
            if (epoll_fd_ < 0) {
                // TODO: Log error
                reactor_state_.store(ReactorState::ERROR_EPOLL_FD);
                return false;
            }

            reactor_state_.store(ReactorState::RUNNING);
            return true;
        }

        /**
         * @brief
         * Runs one reactor iteration.
         *
         * This flushes pending attachments, flushes pending detach callbacks,
         * and waits for epoll events when the reactor is running.
         */
        void EventPollReactor::run() {
            if (!in_running_state()) {
                if (in_error_state()) {
                    // TODO: Log error.
                }

                else if (reactor_state_.load() == ReactorState::STOPPING) {
                    // TODO: Log STOPPING.
                    // TODO: Stop events.
                }
                return;
            }

            flush_detaching_subscriptions();
            wait_for_events();
        }

        /**
         * @brief
         * Requests the reactor to stop.
         */
        void EventPollReactor::stop() noexcept {
            reactor_state_.store(ReactorState::STOPPING);
        }

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
        bool EventPollReactor::attach(shared::EventPollFlag interest,
                                      EventPollListener* listener,
                                      EventPollSubscriber* subscriber) {
            std::size_t max_size = EPOLL_SUBSCRIPTION_SIZE;
            const std::size_t available_size = max_size - subscriptions_.size();
            if (available_size < 0) {
                // TODO: Log error
                return false;
            }

            if (!subscriber->has_valid_fd()) {
                // TODO: Log error
                return false;
            }

            const int fd = subscriber->fd();
            if (subscriptions_.contains(fd)) {
                // TODO: Log error
                return false;
            }

            for (auto& subscription : pending_attach_subscriptions_) {
                if (subscription.subscriber->fd() == subscriber->fd()) {
                    // TODO: Log error.
                    return;
                }
            }

            pending_attach_subscriptions_.push_back(Subscription{interest, listener, subscriber});

            // TODO: Log info
            return true;
        }

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
        bool EventPollReactor::modify(int fd, shared::EventPollFlag interest) {
            auto iterator = subscriptions_.find(fd);
            if (iterator == subscriptions_.end()) {
                // TODO: Log error
                return false;
            }

            Subscription& subscription = iterator->second;
            const std::uint32_t event = impetus::shared::to_epoll_native(interest);
            bool success = epoll_mod(fd, event);
            if (success) {
                subscription.flag = interest;
            }
            return success;
        }

        /**
         * @brief
         * Waits for epoll events and dispatches them.
         */
        void EventPollReactor::wait_for_events() {
            const int max_events = EPOLL_MAX_EVENTS;
            const int timeout_ms = EPOLL_TIMEOUT_MS;
            epoll_event events[max_events];

            const int active_count = ::epoll_wait(epoll_fd_, events, max_events, timeout_ms);
            if (active_count < 0) {
                int error = errno;
                // TODO: Log error
                if (error == EINTR) {
                    return;
                }
            }

            else if (active_count > 0) {
                dispatch_events(events, active_count);
            }
        }

        /**
         * @brief
         * Dispatches epoll events to their corresponding subscribers.
         *
         * @param events
         * Array of epoll events returned by epoll_wait().
         * @param count
         * Number of valid entries in the array.
         */
        void EventPollReactor::dispatch_events(epoll_event* events, int count) {
            for (int index = 0; index < count; ++index) {
                const int fd = events[index].data.fd;
                const std::uint32_t event = events[index].events;

                auto iterator = subscriptions_.find(fd);
                if (iterator == subscriptions_.end()) {
                    // TODO: Log debug
                    continue;
                }

                EventPollSubscriber* subscriber = iterator->second.subscriber;
                if ((event & (EPOLLERR | EPOLLHUP)) != 0U) {
                    subscriber->on_error();
                    continue;
                }

                if ((event & EPOLLIN) != 0U) {
                    subscriber->on_read();
                }

                if ((event & EPOLLOUT) != 0U) {
                    subscriber->on_write();
                }
            }
        }

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
        bool EventPollReactor::detach_subscriber(int fd) {
            auto iterator = subscriptions_.find(fd);
            if (iterator == subscriptions_.end()) {
                return false;
            }

            Subscription subscription = iterator->second;
            if (subscription.listener && subscription.subscriber) {
                subscription.listener->on_detaching(*subscription.subscriber);
            }

            epoll_del(fd);
            subscriptions_.erase(iterator);
            pending_detached_subscriptions_.push_back(subscription);
            return true;
        }

        /**
         * @brief
         * Flushes all subscribers queued for attachment.
         */
        void EventPollReactor::flush_attaching_subscriptions() {
            if (pending_attach_subscriptions_.empty()) {
                return;
            }

            std::vector<Subscription> local;
            local.swap(pending_attach_subscriptions_);

            for (auto& subscription : local) {
                if (subscription.listener == nullptr || subscription.subscriber == nullptr) {
                    // TODO: Log error.
                    continue;
                }

                const int fd = subscription.subscriber->fd();
                if (subscriptions_.contains(fd)) {
                    // TODO: Log error
                    continue;
                }

                const std::uint32_t event = impetus::shared::to_epoll_native(subscription.flag);
                if (!epoll_add(fd, event)) {
                    // TODO: Log error.
                    continue;
                }

                subscriptions_.try_emplace(fd, subscription);
                subscription.listener->on_attached(*subscription.subscriber);
            }
        }

        /**
         * @brief
         * Flushes all subscribers queued for detach-complete callbacks.
         */
        void EventPollReactor::flush_detaching_subscriptions() {
            if (pending_detached_subscriptions_.empty()) {
                return;
            }

            std::vector<Subscription> local;
            local.swap(pending_detached_subscriptions_);
            for (auto& subscription : local) {
                if (subscription.listener == nullptr || subscription.listener == nullptr) {
                    // TODO: Log error.
                    continue;
                }

                subscription.listener->on_detached(*subscription.subscriber);
            }
        }

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
        bool EventPollReactor::set_reactor_name(std::string_view suffix) {
            // Enforce both debug and release.
            int limit = REACTOR_NAME_LIMIT;
            assert(suffix.size() <= limit && "Reactor suffix must not be over 4 characters");
            if (suffix.size() > limit) {
                return false;
            }

            // TODO: Guard for total length

            std::size_t prefix_len = S_REACTOR_NAME_PREFIX.size();
            std::memcpy(reactor_name_, S_REACTOR_NAME_PREFIX.data(), prefix_len);
            std::memcpy(reactor_name_ + prefix_len, suffix.data(), suffix.size());
            std::size_t total_len = prefix_len + suffix.size();
            reactor_name_[total_len] = '\0';
            return true;
        }

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
        bool EventPollReactor::epoll_add(int fd, std::uint32_t events) noexcept {
            epoll_event ev{};
            ev.events = events;
            ev.data.fd = fd;

            return ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
        }

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
        bool EventPollReactor::epoll_mod(int fd, std::uint32_t events) noexcept {
            epoll_event ev{};
            ev.events = events;
            ev.data.fd = fd;

            return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
        }

        /**
         * @brief
         * Removes a file descriptor from epoll.
         *
         * @param fd
         * File descriptor to remove.
         */
        void EventPollReactor::epoll_del(int fd) noexcept {
            ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        }
    }
}