#pragma once

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Base class for all reactor subscribers.
         *
         * A subscriber receives I/O callbacks from the reactor for its bound file descriptor.
         * Lifetime remains owned by the library user.
         */
        class EventPollSubscriber {
            public:
            /**
             * @brief
             * Standard invalid file descriptor value.
             */
            static constexpr int K_INVALID_FD = -1;

            EventPollSubscriber(const EventPollSubscriber&) = delete;
            EventPollSubscriber& operator=(const EventPollSubscriber&) = delete;
            EventPollSubscriber(EventPollSubscriber&&) = delete;
            EventPollSubscriber& operator=(EventPollSubscriber&&) = delete;

            virtual ~EventPollSubscriber() = default;

            public:
            /**
             * @brief
             * Retrieves the subscriber file descriptor.
             *
             * @return
             * Bound file descriptor.
             */
            [[nodiscard]]
            int fd() const noexcept {
                return fd_;
            }

            /**
             * @brief
             * Checks whether the subscriber holds a valid file descriptor.
             *
             * @return
             * TRUE if the file descriptor is valid, FALSE otherwise.
             */
            [[nodiscard]]
            bool has_valid_fd() const noexcept {
                return fd_ != K_INVALID_FD;
            }

            /**
             * @brief
             * Called when the file descriptor becomes readable.
             */
            virtual void on_read() noexcept {
            }

            /**
             * @brief
             * Called when the file descriptor becomes writable.
             */
            virtual void on_write() noexcept {
            }

            /**
             * @brief
             * Called after the subscriber has been attached successfully.
             */
            virtual void on_attached() noexcept {
            }

            /**
             * @brief
             * Called after the subscriber has been fully detached.
             */
            virtual void on_detached() noexcept {
            }

            /**
             * @brief
             * Called when the reactor detects an error or hangup event.
             */
            virtual void on_error() noexcept = 0;

            protected:
            /**
             * @brief
             * Constructs the subscriber with a file descriptor.
             *
             * @param fd
             * File descriptor bound to the subscriber.
             */
            explicit EventPollSubscriber(int fd) noexcept
                : fd_(fd) {
            }

            /**
             * @brief
             * Marks the subscriber file descriptor as invalid.
             */
            void invalidate_fd() noexcept {
                fd_ = K_INVALID_FD;
            }

            private:
            /**
             * @brief
             * The file descriptor bound to this subscriber.
             */
            int fd_ = K_INVALID_FD;
        };
    }
}