#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include "i_consumer.hpp"

namespace impetus {
    namespace dispatch {
        /**
         * @brief
         * Abstract base for a producer that owns and fans out items of type T
         * to registered IConsumer<T> instances.
         *
         * add_consumer() and remove_consumer() only queue the request — they
         * never touch the live consumer list directly. dispatch() locks just
         * long enough to apply queued additions and removals, then copy the
         * resulting consumer list into a snapshot of shared_ptr. The lock is
         * released before consume() is called on anything.
         *
         * The snapshot holds shared_ptr, not raw pointers into consumers_:
         * if another thread's dispatch() concurrently flushes a removal for
         * an object this snapshot already copied, that object stays alive —
         * the snapshot's own reference keeps it valid — until this dispatch()
         * finishes using it. add_consumer() and remove_consumer() still take
         * and return unique-ownership semantics from the caller's point of
         * view (a caller hands over sole ownership and keeps nothing); the
         * shared_ptr is an internal detail that exists only to make a brief,
         * safe copy for the unlocked consume() phase, not shared ownership
         * in the design sense.
         *
         * Thread-safe: add_consumer(), remove_consumer(), and dispatch() may
         * be called from any thread, including reentrantly — a consumer's
         * consume() may call dispatch() again on this same producer (directly
         * or indirectly) without deadlocking, since mutex_ is never held
         * while consume() runs.
         */
        template <typename T>
        class IProducer {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            IProducer(const IProducer&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            IProducer& operator=(const IProducer&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            IProducer(IProducer&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            IProducer& operator=(IProducer&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~IProducer() = default;

            public:
            /**
             * @brief
             * Queues a consumer for addition. Takes ownership. The consumer
             * is inserted into the live list on the next dispatch() call and
             * receives that same dispatch.
             *
             * @param consumer
             * Consumer to take ownership of and register.
             */
            void add_consumer(std::unique_ptr<IConsumer<T>> consumer) {
                if (!consumer) {
                    return;
                }

                std::lock_guard lock(mutex_);
                pending_add_.push_back(std::shared_ptr<IConsumer<T>>(std::move(consumer)));
            }

            /**
             * @brief
             * Queues a previously registered consumer for removal. The
             * consumer is erased on the next dispatch() call, and destroyed
             * once every in-flight dispatch() snapshot referencing it finishes.
             *
             * @param consumer
             * Pointer identifying the consumer to remove, as returned by the
             * unique_ptr passed to add_consumer() at registration time.
             */
            void remove_consumer(IConsumer<T>* consumer) {
                if (!consumer) {
                    return;
                }

                std::lock_guard lock(mutex_);
                pending_remove_.push_back(consumer);
            }

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            IProducer() = default;

            /**
             * @brief
             * Applies all queued additions, then all queued removals, then
             * copies the resulting consumer list into a snapshot — all under
             * mutex_. Releases the lock, then calls consume() on every
             * consumer in the snapshot. Called by subclasses when they have
             * a new item to dispatch.
             *
             * @param item
             * Item to send to all consumers.
             */
            void dispatch(const T& item) noexcept {
                std::vector<std::shared_ptr<IConsumer<T>>> snapshot;
                {
                    std::lock_guard lock(mutex_);

                    for (auto& consumer : pending_add_) {
                        consumers_.push_back(std::move(consumer));
                    }
                    pending_add_.clear();

                    for (auto* target : pending_remove_) {
                        consumers_.erase(
                            std::remove_if(consumers_.begin(),
                                           consumers_.end(),
                                           [target](const std::shared_ptr<IConsumer<T>>& c) {
                                               return c.get() == target;
                                           }),
                            consumers_.end());
                    }
                    pending_remove_.clear();

                    snapshot = consumers_;
                }

                for (auto& consumer : snapshot) {
                    consumer->consume(item);
                }
            }

            private:
            /**
             * @brief
             * Guards consumers_, pending_add_, and pending_remove_.
             */
            std::mutex mutex_;

            /**
             * @brief
             * Currently registered consumers. Reactor-thread-only outside of
             * dispatch()'s locked flush step — only mutated there.
             */
            std::vector<std::shared_ptr<IConsumer<T>>> consumers_;

            /**
             * @brief
             * Consumers queued for addition. Protected by mutex_.
             */
            std::vector<std::shared_ptr<IConsumer<T>>> pending_add_;

            /**
             * @brief
             * Raw pointers identifying consumers queued for removal. Protected by mutex_.
             */
            std::vector<IConsumer<T>*> pending_remove_;
        };
    }
}
