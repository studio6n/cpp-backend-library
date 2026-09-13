#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "../buffer/buffer_utility.hpp"
#include "base_socket.hpp"

namespace impetus {
    namespace network {
        class TcpSocket;

        /**
         * @brief
         * Base class for handling one TCP connection's data.
         *
         * Owns the connection it handles — one instance per connection, which
         * is what gives every callback an implicit identity and removes the
         * need to pass the socket through every call.
         *
         * Stateful by design, so it carries no I prefix: the fd is not
         * incidental state bolted onto an interface, it is what makes this
         * object *be* a particular connection's handler. Same reasoning as
         * EventPollSubscriber, which also holds its own fd.
         *
         * Subclasses implement on_data(); the rest are optional hooks.
         *
         * Lifetime note: closing the connection (close(), a peer disconnect,
         * or an error) does not destroy this object or the socket it owns —
         * the fd is closed and detached, but both objects stay alive until
         * the owner destroys them. Destroying a TcpChannelListener from
         * inside one of its own callbacks is undefined; defer it instead.
         */
        class TcpChannelListener {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            TcpChannelListener(const TcpChannelListener&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            TcpChannelListener& operator=(const TcpChannelListener&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            TcpChannelListener(TcpChannelListener&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            TcpChannelListener& operator=(TcpChannelListener&&) = delete;

            /**
             * @brief
             * Destroys the owned socket, which closes its fd.
             */
            virtual ~TcpChannelListener();

            public:
            /**
             * @brief
             * Takes ownership of an already-connected fd — typically one
             * delivered by ITcpConnectorListener::on_accepted() — and
             * attaches it to the named reactor.
             *
             * @param fd
             * Connected file descriptor to take ownership of.
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param read_config
             * Buffer sizing for received data.
             *
             * @param write_config
             * Buffer sizing for outgoing data.
             *
             * @return
             * True if the socket was created and attached. On false, the fd
             * has been closed and this handler holds nothing.
             */
            [[nodiscard]]
            bool adopt(int fd,
                       std::string_view reactor_name,
                       BufferConfig read_config = {},
                       BufferConfig write_config = {});

            /**
             * @brief
             * Takes ownership of a fresh fd and actively connects it to
             * ip:port. Completion is asynchronous — on_connected() fires once
             * the handshake finishes, on_closed() if it fails.
             *
             * @param fd
             * File descriptor from ::socket(AF_INET, SOCK_STREAM, 0).
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param ip
             * Remote address, network byte order.
             *
             * @param port
             * Remote port, host byte order.
             *
             * @param read_config
             * Buffer sizing for received data.
             *
             * @param write_config
             * Buffer sizing for outgoing data.
             *
             * @return
             * True if the connect attempt started and the socket attached.
             * Not an indication that the connection is established yet.
             */
            [[nodiscard]]
            bool connect(int fd,
                         std::string_view reactor_name,
                         std::uint32_t ip,
                         std::uint16_t port,
                         BufferConfig read_config = {},
                         BufferConfig write_config = {});

            /**
             * @brief
             * Queues data to be sent on this connection.
             *
             * @param data
             * Source bytes to send.
             *
             * @param size
             * Number of bytes to send.
             *
             * @return
             * BufferCode::OK on success, BufferCode::ERR_FULL if the write
             * buffer is out of room, BufferCode::NOT_REQUIRED if this handler
             * holds no open connection.
             */
            [[nodiscard]]
            buffer::BufferCode write(const char* data, std::size_t size) noexcept;

            /**
             * @brief
             * Returns the fd of the owned connection, or K_INVALID_FD if this
             * handler holds nothing or the connection has closed.
             */
            [[nodiscard]]
            int fd() const noexcept;

            /**
             * @brief
             * Returns true if this handler holds an open connection.
             */
            [[nodiscard]]
            bool is_open() const noexcept;

            /**
             * @brief
             * Closes the connection. Fires on_closed(). Safe to call from
             * within a callback — the owned socket object survives until this
             * handler is destroyed.
             */
            void close() noexcept;

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            TcpChannelListener() = default;

            protected:
            /**
             * @brief
             * Called whenever bytes are available. The buffer is not cleared
             * between calls — return how many leading bytes were consumed and
             * they are erased; anything left over stays buffered and is
             * presented again, prefixed to the next arrival. Return 0 to keep
             * the whole buffer and wait for more.
             *
             * @param data
             * Pointer to the start of the buffered bytes.
             *
             * @param size
             * Number of bytes currently buffered.
             *
             * @return
             * Number of leading bytes consumed. Values above size are clamped.
             */
            [[nodiscard]]
            virtual std::size_t on_data(const char* data, std::size_t size) noexcept
                = 0;

            /**
             * @brief
             * Called once an outbound connect() has completed successfully.
             * Never fires for an adopted connection, which is already live.
             */
            virtual void on_connected() noexcept {
            }

            /**
             * @brief
             * Called once every queued byte has been flushed to the kernel.
             * Useful for backpressure — safe to queue more from here.
             */
            virtual void on_writable() noexcept {
            }

            /**
             * @brief
             * Called once the connection has closed — peer disconnect, local
             * error, a failed connect(), or close(). No further on_data()
             * calls follow.
             */
            virtual void on_closed() noexcept {
            }

            private:
            friend class TcpSocket;

            private:
            /**
             * @brief
             * The connection this handler owns. Null until adopt() or
             * connect() succeeds.
             */
            std::unique_ptr<TcpSocket> socket_;
        };
    }
}
