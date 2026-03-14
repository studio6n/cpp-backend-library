#pragma once

#include "event_poll_subscriber.hpp"

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Listener interface for subscriber lifecycle notifications.
         *
         * A listener observes attachment and detachment events emitted by the reactor.
         */
        class EventPollListener {
            public:
            EventPollListener(const EventPollListener&) = delete;
            EventPollListener& operator=(const EventPollListener&) = delete;
            EventPollListener(EventPollListener&&) = delete;
            EventPollListener& operator=(EventPollListener&&) = delete;

            virtual ~EventPollListener() = default;

            public:
            /**
             * @brief
             * Called after a subscriber is attached successfully.
             *
             * @param subscriber
             * Attached subscriber.
             */
            virtual void on_attached(EventPollSubscriber& subscriber) noexcept {
                subscriber.on_detached();
            }

            /**
             * @brief
             * Called when a subscriber is being detached.
             *
             * @param subscriber
             * Subscriber being detached.
             */
            virtual void on_detaching(EventPollSubscriber& subscriber) noexcept = 0;

            /**
             * @brief
             * Called after a subscriber has been fully detached.
             *
             * @param subscriber
             * Detached subscriber.
             */
            virtual void on_detached(EventPollSubscriber& subscriber) noexcept {
                subscriber.on_detached();
            }

            protected:
            /**
             * @brief
             * Constructs the listener base.
             */
            EventPollListener() = default;
        };
    }
}