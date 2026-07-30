#pragma once

#include "event_poll_subscriber.hpp"

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Interface for subscriber lifecycle notifications.
         *
         * Implement this to observe when subscribers are attached to or detached
         * from the reactor. on_attached() and on_detached() have default
         * implementations that forward directly to the subscriber.
         */
        class IEventPollListener {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            IEventPollListener(const IEventPollListener&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            IEventPollListener& operator=(const IEventPollListener&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            IEventPollListener(IEventPollListener&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            IEventPollListener& operator=(IEventPollListener&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~IEventPollListener() = default;

            public:
            /**
             * @brief
             * Called after a subscriber is successfully attached to the reactor.
             *
             * @param subscriber
             * The attached subscriber.
             */
            virtual void on_attached(EventPollSubscriber& subscriber) noexcept {
                subscriber.on_attached();
            }

            /**
             * @brief
             * Called when a subscriber is about to be detached from the reactor.
             * Must be implemented — use this to release any state tied to the subscriber.
             *
             * @param subscriber
             * The subscriber being detached.
             */
            virtual void on_detaching(EventPollSubscriber& subscriber) noexcept = 0;

            /**
             * @brief
             * Called after a subscriber has been fully detached from the reactor.
             *
             * @param subscriber
             * The detached subscriber.
             */
            virtual void on_detached(EventPollSubscriber& subscriber) noexcept {
                subscriber.on_detached();
            }

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            IEventPollListener() = default;
        };
    }
}
