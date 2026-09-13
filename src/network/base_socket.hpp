#pragma once

#include <string_view>

#include "../buffer/socket_buffer.hpp"
#include "../poll/event_poll_subscriber.hpp"
#include "../poll/i_event_poll_listener.hpp"

namespace impetus {
    namespace network {
        /**
         * @brief
         * Buffer size configuration for a single I/O direction.
         */
        struct BufferConfig {
            /**
             * @brief
             * Block size in bytes.
             */
            buffer::SocketBuffer::Memory memory = buffer::SocketBuffer::Memory::MEDIUM;

            /**
             * @brief
             * Number of blocks to allocate.
             */
            buffer::SocketBuffer::Capacity capacity = buffer::SocketBuffer::Capacity::MEDIUM;
        };

        /**
         * @brief
         * Shared file descriptor ownership for every socket role.
         *
         * Owns the fd and closes it on destruction. Forces the fd into
         * non-blocking mode at construction — every read and write path in
         * this module assumes that and relies on EAGAIN to terminate loops.
         *
         * Deliberately thin: no buffers, no protocol behaviour, no knowledge
         * of which reactor anything is attached to. Not directly usable —
         * on_error() remains pure virtual, inherited from EventPollSubscriber.
         *
         * Also serves as its own IEventPollListener. The reactor requires a
         * non-null listener for every subscription — it silently drops any
         * attach whose listener is null — and a socket has no separate
         * lifecycle observer to supply, so it fills that role itself.
         */
        class BaseSocket : public poll::EventPollSubscriber, public poll::IEventPollListener {
            public:
            /**
             * @brief
             * Default construction is disabled — an fd is a hard requirement.
             */
            BaseSocket() = delete;

            /**
             * @brief
             * Copy construction is disabled.
             */
            BaseSocket(const BaseSocket&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            BaseSocket& operator=(const BaseSocket&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            BaseSocket(BaseSocket&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            BaseSocket& operator=(BaseSocket&&) = delete;

            /**
             * @brief
             * Closes the owned file descriptor if it is still open.
             */
            ~BaseSocket() override;

            public:
            /**
             * @brief
             * Required by IEventPollListener. A socket manages its own
             * teardown through begin_shutdown(), so there is nothing extra to
             * do when the reactor detaches it.
             *
             * @param subscriber
             * The subscriber being detached — always this socket.
             */
            void on_detaching(poll::EventPollSubscriber& subscriber) noexcept override {
                (void) subscriber;
            }

            protected:
            /**
             * @brief
             * Takes ownership of fd and forces it into non-blocking mode.
             * Does nothing else — no bind, no connect, no reactor call.
             *
             * @param fd
             * File descriptor to take ownership of.
             */
            explicit BaseSocket(int fd) noexcept;

            protected:
            /**
             * @brief
             * Detaches from EventPollRegistry::instance() and closes the fd.
             *
             * Ordering matters: detach() reads fd() internally to find its
             * entry, so invalidate_fd() must come after it, never before.
             *
             * Idempotent — a second call is a no-op, which is what makes it
             * safe to call from on_error() even if on_error() fires more than
             * once before the object is actually destroyed.
             *
             * @param reactor_name
             * Reactor to detach from. Not stored by this class — the derived
             * socket owns that, since only it knows where it was attached.
             */
            void begin_shutdown(std::string_view reactor_name) noexcept;

            /**
             * @brief
             * Returns true once begin_shutdown() has run.
             */
            [[nodiscard]]
            bool is_closing() const noexcept {
                return closing_;
            }

            private:
            /**
             * @brief
             * Set by begin_shutdown(). Guards against tearing down twice.
             */
            bool closing_ = false;
        };
    }
}
