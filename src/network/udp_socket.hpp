#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/socket.h>

#include "base_socket.hpp"

namespace impetus {
    namespace network {
        class IUdpListener;

        /**
         * @brief
         * A UDP socket, in whichever role its setup call assigned it.
         *
         * bind() leaves the socket unconnected, so it receives from and can
         * reply to any peer. connect() locks it to exactly one peer — that is
         * a local kernel-side filter, not a handshake, so unlike TCP it
         * resolves immediately and has no connected callback.
         *
         * Final: extend behaviour through IUdpListener, not by subclassing.
         */
        class UdpSocket final : public BaseSocket {
            public:
            /**
             * @brief
             * Default construction is disabled — an fd is a hard requirement.
             */
            UdpSocket() = delete;

            /**
             * @brief
             * Copy construction is disabled.
             */
            UdpSocket(const UdpSocket&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            UdpSocket& operator=(const UdpSocket&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            UdpSocket(UdpSocket&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            UdpSocket& operator=(UdpSocket&&) = delete;

            /**
             * @brief
             * Takes ownership of an fd from an external ::socket() call. Does
             * no setup — call bind() or connect() for that.
             *
             * @param fd
             * File descriptor to take ownership of.
             *
             * @param read_config
             * Buffer sizing for received datagrams. Must be at least as large
             * as the biggest datagram expected, since anything longer is
             * truncated by the kernel.
             */
            explicit UdpSocket(int fd, BufferConfig read_config = {}) noexcept;

            /**
             * @brief
             * Default destructor. The base class closes the fd.
             */
            ~UdpSocket() override = default;

            public:
            /**
             * @brief
             * Binds to ip:port and attaches to the reactor. The socket stays
             * unconnected and will receive from any sender.
             *
             * @param ip
             * Local address to bind to, network byte order (0 for any).
             *
             * @param port
             * Local port to bind to, host byte order.
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param listener
             * Notified of received datagrams. Must outlive this socket.
             *
             * @return
             * True if bind and the reactor attach both succeeded.
             */
            [[nodiscard]]
            bool bind(std::uint32_t ip,
                      std::uint16_t port,
                      std::string_view reactor_name,
                      IUdpListener& listener);

            /**
             * @brief
             * Locks this socket to ip:port and attaches to the reactor. After
             * this only that peer's datagrams are delivered, and send() may be
             * used instead of sendto().
             *
             * @param ip
             * Remote address, network byte order.
             *
             * @param port
             * Remote port, host byte order.
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param listener
             * Notified of received datagrams. Must outlive this socket.
             *
             * @return
             * True if connect and the reactor attach both succeeded.
             */
            [[nodiscard]]
            bool connect(std::uint32_t ip,
                         std::uint16_t port,
                         std::string_view reactor_name,
                         IUdpListener& listener);

            /**
             * @brief
             * Sends a datagram to an explicit destination. Valid in either
             * role, though a connected socket may only address its own peer.
             *
             * @param data
             * Source bytes to send.
             *
             * @param size
             * Number of bytes to send.
             *
             * @param destination
             * Destination address.
             *
             * @param destination_len
             * Length of destination.
             *
             * @return
             * True if the whole datagram was handed to the kernel.
             */
            [[nodiscard]]
            bool write(const char* data,
                       std::size_t size,
                       const sockaddr* destination,
                       socklen_t destination_len) noexcept;

            /**
             * @brief
             * Sends a datagram to the connected peer. Only valid after
             * connect().
             *
             * @param data
             * Source bytes to send.
             *
             * @param size
             * Number of bytes to send.
             *
             * @return
             * True if the whole datagram was handed to the kernel.
             */
            [[nodiscard]]
            bool write(const char* data, std::size_t size) noexcept;

            /**
             * @brief
             * Returns true if this socket is locked to a single peer.
             */
            [[nodiscard]]
            bool is_connected() const noexcept {
                return connected_;
            }

            /**
             * @brief
             * Detaches, closes the fd, and notifies the listener. Idempotent.
             * The object itself stays alive — its owner destroys it.
             */
            void close() noexcept;

            private:
            /**
             * @brief
             * Called by the reactor when the fd is readable. Receives
             * datagrams until EAGAIN.
             */
            void on_read() noexcept override;

            /**
             * @brief
             * Called by the reactor when the fd is writable. Datagrams are
             * sent directly from write(), so there is nothing to flush.
             */
            void on_write() noexcept override {
            }

            /**
             * @brief
             * Called by the reactor on error or hangup. Tears down.
             */
            void on_error() noexcept override;

            private:
            /**
             * @brief
             * Attaches to the reactor and records the listener.
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param listener
             * Listener to record.
             *
             * @return
             * True if the attach succeeded.
             */
            [[nodiscard]]
            bool attach(std::string_view reactor_name, IUdpListener& listener);

            /**
             * @brief
             * Tears down and notifies the listener.
             */
            void shutdown_and_notify() noexcept;

            private:
            /**
             * @brief
             * Reactor this socket attached to, needed to detach on teardown.
             */
            std::string reactor_name_;

            /**
             * @brief
             * Non-owning. Notified of received datagrams and closure.
             */
            IUdpListener* listener_ = nullptr;

            /**
             * @brief
             * Staging buffer for the datagram currently being received.
             */
            buffer::SocketBuffer read_buffer_;

            /**
             * @brief
             * Sender of the most recently received datagram.
             */
            sockaddr_storage sender_addr_;

            /**
             * @brief
             * Length of sender_addr_.
             */
            socklen_t sender_len_;

            /**
             * @brief
             * True once connect() has locked this socket to one peer.
             */
            bool connected_ = false;
        };
    }
}
