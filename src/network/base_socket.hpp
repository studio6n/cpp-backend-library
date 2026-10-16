#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "../buffer/socket_buffer.hpp"
#include "../poll/event_poll_flag.hpp"
#include "../poll/event_poll_subscriber.hpp"

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
         * Owns the fd until a reactor accepts it; from then on the reactor
         * closes it, after removing it. Forces the fd into non-blocking mode
         * at construction — every read and write path in this module assumes
         * that and relies on EAGAIN to terminate loops.
         *
         * Must be owned by a std::shared_ptr (std::make_shared): the reactor
         * keeps only a weak_ptr to it, taken with weak_from_this(). A socket
         * owned any other way cannot attach.
         *
         * Deliberately thin: no buffers, no protocol behaviour. Not directly
         * usable — on_error() remains pure virtual, inherited from
         * EventPollSubscriber.
         */
        class BaseSocket :
            public poll::EventPollSubscriber,
            public std::enable_shared_from_this<BaseSocket> {
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
             * Shuts the socket down if that has not happened yet. Destroying an
             * attached socket without close() is an error and is logged.
             */
            ~BaseSocket() override;

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
             * Attaches this socket to the named reactor through
             * EventPollRegistry::instance(). Once this returns true the reactor
             * owns closing the fd.
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param interest
             * Epoll interest flags.
             *
             * @return
             * True if the reactor accepted the fd. False if this socket is not
             * owned by a std::shared_ptr, is already attached or closing, or the
             * reactor was not found or refused it.
             */
            [[nodiscard]]
            bool attach_to_reactor(std::string_view reactor_name, poll::EventPollFlag interest);

            /**
             * @brief
             * Returns the name of the reactor that accepted the fd, or an empty
             * string if none did.
             */
            [[nodiscard]]
            const std::string& reactor_name() const noexcept {
                return reactor_name_;
            }

            /**
             * @brief
             * Stops using the fd. If a reactor accepted it, hands it back with
             * detach() and the reactor closes it once it has removed it, so the
             * number stays taken until no reactor entry refers to it. If no
             * reactor ever accepted it, closes it here.
             *
             * Idempotent — a second call is a no-op, which is what makes it
             * safe to call from on_error() even if on_error() fires more than
             * once before the object is actually destroyed.
             */
            void begin_shutdown() noexcept;

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
             * Reactor that accepted the fd. Empty until attach_to_reactor()
             * succeeds; from then on that reactor closes the fd.
             */
            std::string reactor_name_;

            /**
             * @brief
             * Set by begin_shutdown(). Guards against tearing down twice.
             */
            bool closing_ = false;
        };
    }
}
