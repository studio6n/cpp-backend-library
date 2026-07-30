#include "event_poll_reactor.hpp"

#include <cerrno>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>

namespace {
    constexpr int K_EPOLL_MAX_EVENTS = 100;
    constexpr int K_EPOLL_TIMEOUT_MS = 100;
    constexpr std::size_t K_EPOLL_SUBSCRIPTION_SIZE = 50;
    constexpr const char* REACTOR_NAME_PREFIX = "ep-reactor-";
}

namespace impetus {
    namespace poll {
        EventPollReactor::EventPollReactor(std::string_view suffix)
            : reactor_state_(ReactorState::NOT_STARTED)
            , subscription_count_(0)
            , max_subscriptions_(K_EPOLL_SUBSCRIPTION_SIZE)
            , epoll_fd_(-1) {
            set_reactor_name(suffix);
            subscriptions_.reserve(max_subscriptions_);
            pending_attach_subscriptions_.reserve(max_subscriptions_);
            pending_detached_subscriptions_.reserve(max_subscriptions_);
        }

        EventPollReactor::EventPollReactor(std::string_view suffix, std::size_t max_subscriptions)
            : reactor_state_(ReactorState::NOT_STARTED)
            , subscription_count_(0)
            , max_subscriptions_(max_subscriptions > 0 ? max_subscriptions
                                                       : K_EPOLL_SUBSCRIPTION_SIZE)
            , epoll_fd_(-1) {
            set_reactor_name(suffix);
            subscriptions_.reserve(max_subscriptions_);
            pending_attach_subscriptions_.reserve(max_subscriptions_);
            pending_detached_subscriptions_.reserve(max_subscriptions_);
        }

        EventPollReactor::~EventPollReactor() {
            if (epoll_fd_ >= 0) {
                ::close(epoll_fd_);
                epoll_fd_ = -1;
            }
        }

        bool EventPollReactor::start() {
            epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
            if (epoll_fd_ < 0) {
                reactor_state_.store(ReactorState::ERROR_EPOLL_FD);
                return false;
            }

            reactor_state_.store(ReactorState::RUNNING);
            return true;
        }

        void EventPollReactor::run() {
            std::vector<Subscription> local_attach;
            std::vector<int> local_detach;
            std::vector<PendingModify> local_modify;
            {
                std::lock_guard lock(pending_mutex_);
                local_attach.swap(pending_attach_subscriptions_);
                local_detach.swap(pending_detach_fds_);
                local_modify.swap(pending_modify_entries_);
            }

            flush_attach_requests(local_attach);
            flush_modify_requests(local_modify);
            flush_detach_requests(local_detach);

            if (!in_running_state()) {
                return;
            }

            flush_detached_callbacks();
            wait_for_events();
        }

        void EventPollReactor::stop() noexcept {
            reactor_state_.store(ReactorState::STOPPING);
        }

        bool EventPollReactor::attach(EventPollFlag interest,
                                      IEventPollListener* listener,
                                      EventPollSubscriber* subscriber) {
            if (!subscriber->has_valid_fd()) {
                return false;
            }

            const int fd = subscriber->fd();

            std::lock_guard lock(pending_mutex_);

            if (subscription_count_.load() + pending_attach_subscriptions_.size()
                >= max_subscriptions_) {
                return false;
            }

            for (const auto& sub : pending_attach_subscriptions_) {
                if (sub.subscriber->fd() == fd) {
                    return false;
                }
            }

            pending_attach_subscriptions_.push_back(Subscription{interest, listener, subscriber});
            return true;
        }

        void EventPollReactor::detach(const EventPollSubscriber& subscriber) {
            std::lock_guard lock(pending_mutex_);
            pending_detach_fds_.push_back(subscriber.fd());
        }

        void EventPollReactor::modify(int fd, EventPollFlag interest) {
            std::lock_guard lock(pending_mutex_);
            pending_modify_entries_.push_back(PendingModify{fd, interest});
        }

        void EventPollReactor::wait_for_events() {
            epoll_event events[K_EPOLL_MAX_EVENTS];
            const int active_count
                = ::epoll_wait(epoll_fd_, events, K_EPOLL_MAX_EVENTS, K_EPOLL_TIMEOUT_MS);

            if (active_count < 0) {
                // EINTR means a signal interrupted the wait — not a real error, retry next iteration.
                if (errno == EINTR) {
                    return;
                }
                return;
            }

            if (active_count > 0) {
                dispatch_events(events, active_count);
            }
        }

        void EventPollReactor::dispatch_events(epoll_event* events, int count) {
            for (int i = 0; i < count; ++i) {
                const int fd = events[i].data.fd;
                const std::uint32_t event = events[i].events;

                auto it = subscriptions_.find(fd);
                if (it == subscriptions_.end()) {
                    continue;
                }

                EventPollSubscriber* subscriber = it->second.subscriber;
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

        void EventPollReactor::flush_attach_requests(std::vector<Subscription>& local) {
            for (auto& sub : local) {
                if (sub.listener == nullptr || sub.subscriber == nullptr) {
                    continue;
                }

                const int fd = sub.subscriber->fd();
                if (subscriptions_.contains(fd)) {
                    continue;
                }

                const std::uint32_t event = to_epoll_native(sub.flag);
                if (!epoll_add(fd, event)) {
                    continue;
                }

                subscriptions_.try_emplace(fd, sub);
                subscription_count_.fetch_add(1, std::memory_order_relaxed);
                sub.listener->on_attached(*sub.subscriber);
            }
        }

        void EventPollReactor::flush_modify_requests(std::vector<PendingModify>& local) {
            for (const auto& entry : local) {
                auto it = subscriptions_.find(entry.fd);
                if (it == subscriptions_.end()) {
                    continue;
                }

                const std::uint32_t event = to_epoll_native(entry.flag);
                if (!epoll_mod(entry.fd, event)) {
                    continue;
                }

                it->second.flag = entry.flag;
            }
        }

        void EventPollReactor::flush_detach_requests(std::vector<int>& local) {
            for (const int fd : local) {
                auto it = subscriptions_.find(fd);
                if (it == subscriptions_.end()) {
                    continue;
                }

                Subscription sub = it->second;

                if (sub.listener && sub.subscriber) {
                    sub.listener->on_detaching(*sub.subscriber);
                }

                epoll_del(fd);
                subscriptions_.erase(it);
                subscription_count_.fetch_sub(1, std::memory_order_relaxed);

                pending_detached_subscriptions_.push_back(sub);
            }
        }

        void EventPollReactor::flush_detached_callbacks() {
            if (pending_detached_subscriptions_.empty()) {
                return;
            }

            // Swap so on_detached() triggering further detachments does not invalidate iteration.
            std::vector<Subscription> local;
            local.swap(pending_detached_subscriptions_);

            for (auto& sub : local) {
                if (sub.listener && sub.subscriber) {
                    sub.listener->on_detached(*sub.subscriber);
                }
            }
        }

        bool EventPollReactor::set_reactor_name(std::string_view suffix) {
            if (suffix.empty()) {
                return false;
            }

            reactor_name_ = std::string(REACTOR_NAME_PREFIX) + std::string(suffix);
            return true;
        }

        bool EventPollReactor::epoll_add(int fd, std::uint32_t events) noexcept {
            epoll_event ev{};
            ev.events = events;
            ev.data.fd = fd;
            return ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
        }

        bool EventPollReactor::epoll_mod(int fd, std::uint32_t events) noexcept {
            epoll_event ev{};
            ev.events = events;
            ev.data.fd = fd;
            return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
        }

        void EventPollReactor::epoll_del(int fd) noexcept {
            ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        }
    }
}
