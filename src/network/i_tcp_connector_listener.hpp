#pragma once

#include <sys/socket.h>

namespace impetus {
    namespace network {
        /**
         * @brief
         * Interface for observing connections accepted by a listening
         * TcpSocket.
         *
         * A listening socket produces file descriptors and nothing else — it
         * never carries application data, so this interface has no data
         * callback. Pure interface: it holds no state, which is why it keeps
         * the I prefix. Contrast TcpChannelListener, which owns its fd.
         */
        class ITcpConnectorListener {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            ITcpConnectorListener(const ITcpConnectorListener&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            ITcpConnectorListener& operator=(const ITcpConnectorListener&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            ITcpConnectorListener(ITcpConnectorListener&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            ITcpConnectorListener& operator=(ITcpConnectorListener&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~ITcpConnectorListener() = default;

            public:
            /**
             * @brief
             * Called once per accepted connection, before the new fd has been
             * attached to anything. The implementer decides which reactor and
             * which TcpChannelListener it gets — typically by calling
             * TcpChannelListener::adopt() on a handler it owns.
             *
             * Taking ownership of fd is the implementer's responsibility. If
             * this call returns without adopting it, the fd leaks.
             *
             * @param fd
             * Newly accepted file descriptor.
             *
             * @param peer
             * Address of the connecting peer. Only valid for this call.
             *
             * @param peer_len
             * Length of peer.
             */
            virtual void
            on_accepted(int fd, const sockaddr_storage& peer, socklen_t peer_len) noexcept
                = 0;

            /**
             * @brief
             * Called once the listening socket itself has closed. No further
             * on_accepted() calls follow.
             */
            virtual void on_closed() noexcept {
            }

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            ITcpConnectorListener() = default;
        };
    }
}
