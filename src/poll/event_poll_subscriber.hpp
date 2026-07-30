#pragma once

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Abstract base class for all epoll reactor subscribers.
         *
         * A subscriber receives I/O callbacks from the reactor for its bound file
         * descriptor. Lifetime is owned by the caller — the reactor holds a non-owning
         * pointer.
         */
        class EventPollSubscriber {
            public:
            /**
             * @brief
             * Sentinel value for an invalid file descriptor.
             */
            static constexpr int K_INVALID_FD = -1;

            /**
             * @brief
             * Copy construction is disabled.
             */
            EventPollSubscriber(const EventPollSubscriber&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            EventPollSubscriber& operator=(const EventPollSubscriber&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            EventPollSubscriber(EventPollSubscriber&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            EventPollSubscriber& operator=(EventPollSubscriber&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~EventPollSubscriber() = default;

            public:
            /**
             * @brief
             * Returns the file descriptor bound to this subscriber.
             */
            [[nodiscard]]
            int fd() const noexcept {
                return fd_;
            }

            /**
             * @brief
             * Returns true if the bound file descriptor is valid.
             */
            [[nodiscard]]
            bool has_valid_fd() const noexcept {
                return fd_ != K_INVALID_FD;
            }

            /**
             * @brief
             * Called by the reactor when the file descriptor becomes readable.
             */
            virtual void on_read() noexcept {
            }

            /**
             * @brief
             * Called by the reactor when the file descriptor becomes writable.
             */
            virtual void on_write() noexcept {
            }

            /**
             * @brief
             * Called after the subscriber has been successfully attached to the reactor.
             */
            virtual void on_attached() noexcept {
            }

            /**
             * @brief
             * Called after the subscriber has been fully detached from the reactor.
             */
            virtual void on_detached() noexcept {
            }

            /**
             * @brief
             * Called when the reactor detects an error or hangup on the file descriptor.
             * Must be implemented by all subclasses.
             */
            virtual void on_error() noexcept = 0;

            protected:
            /**
             * @brief
             * Constructs the subscriber with a file descriptor.
             *
             * @param fd
             * File descriptor to bind to this subscriber.
             */
            explicit EventPollSubscriber(int fd) noexcept
                : fd_(fd) {
            }

            /**
             * @brief
             * Marks the file descriptor as invalid. Call after closing it.
             */
            void invalidate_fd() noexcept {
                fd_ = K_INVALID_FD;
            }

            private:
            /**
             * @brief
             * File descriptor bound to this subscriber.
             */
            int fd_ = K_INVALID_FD;
        };
    }
}
