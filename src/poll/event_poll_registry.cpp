#include "event_poll_registry.hpp"

#include <cassert>

#include "../threads/thread_configuration.hpp"
#include "../threads/worker_thread.hpp"
#include "event_poll_reactor.hpp"
#include "event_poll_subscriber.hpp"
#include "i_event_poll_listener.hpp"

namespace {
    constexpr const char* MAIN_REACTOR_KEY = "main";
}

namespace impetus {
    namespace poll {
        EventPollRegistry& EventPollRegistry::instance() {
            static EventPollRegistry registry;
            return registry;
        }

        /**
         * @brief
         * Pairs a reactor with its WorkerThread. thread is nullptr for the
         * main reactor, which runs on the caller's own thread instead.
         *
         * reactor is shared_ptr, not unique_ptr: attach()/detach()/modify()/run()
         * take a copy of it while holding mutex_, then use it after releasing the
         * lock. If stop()/stop_all() concurrently removes this entry from another
         * thread, the shared_ptr copy keeps the reactor alive until whichever call
         * is using it finishes — the same rationale as IProducer<T>'s snapshot.
         */
        struct EventPollRegistry::ReactorEntry {
            /**
             * @brief
             * The reactor instance.
             */
            std::shared_ptr<EventPollReactor> reactor;

            /**
             * @brief
             * Dedicated worker thread driving this reactor's run() loop.
             * nullptr for the main reactor.
             */
            std::unique_ptr<threads::WorkerThread> thread;
        };

        EventPollRegistry::~EventPollRegistry() {
            stop_all();
        }

        bool EventPollRegistry::init() {
            const std::string key(MAIN_REACTOR_KEY);
            {
                std::lock_guard lock(mutex_);
                if (reactors_.contains(key)) {
                    return false;
                }
            }

            auto entry = std::make_unique<ReactorEntry>();
            entry->reactor = std::make_shared<EventPollReactor>(MAIN_REACTOR_KEY);
            entry->thread = nullptr;

            if (!entry->reactor->start()) {
                return false;
            }

            std::lock_guard lock(mutex_);
            if (reactors_.contains(key)) {
                return false;
            }

            main_thread_id_ = std::this_thread::get_id();
            reactors_.emplace(key, std::move(entry));
            return true;
        }

        bool EventPollRegistry::start(std::string_view name) {
            if (name == MAIN_REACTOR_KEY) {
                // log: "main" is reserved for init() — cannot create a WorkerThread for the main reactor
                return false;
            }

            const std::string key(name);
            {
                std::lock_guard lock(mutex_);
                if (reactors_.contains(key)) {
                    return false;
                }
            }

            auto entry = std::make_unique<ReactorEntry>();
            entry->reactor = std::make_shared<EventPollReactor>(name);

            threads::ThreadConfiguration config;
            config.name = key.size() > 15 ? key.substr(0, 15) : key;
            entry->thread = std::make_unique<threads::WorkerThread>(std::move(config));

            if (!entry->reactor->start()) {
                return false;
            }

            std::shared_ptr<EventPollReactor> reactor = entry->reactor;
            if (!entry->thread->start([reactor] {
                    reactor->run();
                })) {
                return false;
            }

            std::lock_guard lock(mutex_);
            if (reactors_.contains(key)) {
                return false;
            }

            reactors_.emplace(key, std::move(entry));
            return true;
        }

        void EventPollRegistry::stop(std::string_view name) noexcept {
            if (name == MAIN_REACTOR_KEY) {
                // log: cannot stop main reactor individually — use stop_all()
                return;
            }

            std::unique_ptr<ReactorEntry> entry;
            {
                std::lock_guard lock(mutex_);
                auto it = reactors_.find(std::string(name));
                if (it == reactors_.end()) {
                    return;
                }
                entry = std::move(it->second);
                reactors_.erase(it);
            }

            // entry destructs after this scope, outside mutex_ — its WorkerThread's
            // jthread destructor blocks joining the thread; must not happen while
            // holding the lock, or a reactor callback re-entering the registry on
            // that same thread during shutdown would deadlock against this join.
            entry->reactor->stop();
            if (entry->thread) {
                entry->thread->stop();
            }
        }

        void EventPollRegistry::run() noexcept {
            std::shared_ptr<EventPollReactor> reactor;
            {
                std::lock_guard lock(mutex_);
                auto it = reactors_.find(std::string(MAIN_REACTOR_KEY));
                if (it == reactors_.end()) {
                    return;
                }

                assert((main_thread_id_ == std::thread::id{}
                        || std::this_thread::get_id() == main_thread_id_)
                       && "EventPollRegistry::run() must only be called from the thread that "
                          "called init()");

                reactor = it->second->reactor;
            }

            reactor->run();
        }

        void EventPollRegistry::stop_all() noexcept {
            std::unordered_map<std::string, std::unique_ptr<ReactorEntry>> local;
            {
                std::lock_guard lock(mutex_);

                assert((main_thread_id_ == std::thread::id{}
                        || std::this_thread::get_id() == main_thread_id_)
                       && "EventPollRegistry::stop_all() must only be called from the thread "
                          "that called init()");

                local.swap(reactors_);
            }

            // Entries destruct with this function's scope, outside mutex_ — same
            // deadlock-avoidance reasoning as stop().
            for (auto& [name, entry] : local) {
                entry->reactor->stop();
                if (entry->thread) {
                    entry->thread->stop();
                }
            }
        }

        bool EventPollRegistry::attach(std::string_view reactor_name,
                                       EventPollFlag interest,
                                       IEventPollListener* listener,
                                       EventPollSubscriber* subscriber) {
            std::shared_ptr<EventPollReactor> reactor;
            {
                std::lock_guard lock(mutex_);
                auto it = reactors_.find(std::string(reactor_name));
                if (it == reactors_.end()) {
                    return false;
                }
                reactor = it->second->reactor;
            }

            return reactor->attach(interest, listener, subscriber);
        }

        void EventPollRegistry::detach(std::string_view reactor_name,
                                       const EventPollSubscriber& subscriber) {
            std::shared_ptr<EventPollReactor> reactor;
            {
                std::lock_guard lock(mutex_);
                auto it = reactors_.find(std::string(reactor_name));
                if (it == reactors_.end()) {
                    return;
                }
                reactor = it->second->reactor;
            }

            reactor->detach(subscriber);
        }

        void
        EventPollRegistry::modify(std::string_view reactor_name, int fd, EventPollFlag interest) {
            std::shared_ptr<EventPollReactor> reactor;
            {
                std::lock_guard lock(mutex_);
                auto it = reactors_.find(std::string(reactor_name));
                if (it == reactors_.end()) {
                    return;
                }
                reactor = it->second->reactor;
            }

            reactor->modify(fd, interest);
        }
    }
}
