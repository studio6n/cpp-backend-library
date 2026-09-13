#pragma once

#include <cstddef>
#include <sys/socket.h>

namespace impetus {
    namespace network {
        /**
         * @brief
         * Interface for observing datagrams received on a UdpSocket.
         *
         * One interface covers both UDP roles. Unlike TCP there is no accept
         * step and no asynchronous handshake, so a bound socket and a
         * connected one deliver datagrams identically — the only difference
         * is that a connected socket only ever hears from its one peer, so
         * sender is always that same address.
         *
         * Pure interface: it holds no state, so it keeps the I prefix.
         */
        class IUdpListener {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            IUdpListener(const IUdpListener&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            IUdpListener& operator=(const IUdpListener&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            IUdpListener(IUdpListener&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            IUdpListener& operator=(IUdpListener&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~IUdpListener() = default;

            public:
            /**
             * @brief
             * Called once per received datagram, delivered whole. The buffer
             * is cleared after this returns, so there is no partial-message
             * bookkeeping as there is for a TCP stream.
             *
             * @param data
             * Pointer to the datagram bytes.
             *
             * @param size
             * Number of bytes in the datagram.
             *
             * @param sender
             * Address of the sender. Only valid for the duration of this
             * call — copy it if the address is needed afterward.
             *
             * @param sender_len
             * Length of sender.
             */
            virtual void on_datagram(const char* data,
                                     std::size_t size,
                                     const sockaddr_storage& sender,
                                     socklen_t sender_len) noexcept
                = 0;

            /**
             * @brief
             * Called once the socket has closed. No further on_datagram()
             * calls follow.
             */
            virtual void on_closed() noexcept {
            }

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            IUdpListener() = default;
        };
    }
}
