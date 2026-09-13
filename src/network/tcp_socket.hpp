#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../buffer/buffer_utility.hpp"
#include "base_socket.hpp"

namespace impetus {
    namespace network {
        class ITcpConnectorListener;
        class TcpChannelListener;

        /**
         * @brief
         * A TCP socket, in whichever role its setup call assigned it.
         *
         * One class, mirroring the OS: a single fd can listen and accept, or
         * connect and carry data. Which one this instance is is fixed by
         * which setup method was called, and the kernel enforces the same
         * split — a listening fd can never send or receive, and a connected
         * fd can never accept.
         *
         * Final: extend behaviour through ITcpConnectorListener or
         * TcpChannelListener, not by subclassing this.
         */
        class TcpSocket final : public BaseSocket {
            public:
            /**
             * @brief
             * Default construction is disabled — an fd is a hard requirement.
             */
            TcpSocket() = delete;

            /**
             * @brief
             * Copy construction is disabled.
             */
            TcpSocket(const TcpSocket&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            TcpSocket& operator=(const TcpSocket&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            TcpSocket(TcpSocket&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            TcpSocket& operator=(TcpSocket&&) = delete;

            /**
             * @brief
             * Takes ownership of an fd from an external ::socket() call. Does
             * no setup — call listen(), connect(), or attach() for that.
             *
             * @param fd
             * File descriptor to take ownership of.
             *
             * @param read_config
             * Buffer sizing for received data.
             *
             * @param write_config
             * Buffer sizing for outgoing data.
             */
            explicit TcpSocket(int fd,
                               BufferConfig read_config = {},
                               BufferConfig write_config = {}) noexcept;

            /**
             * @brief
             * Default destructor. The base class closes the fd.
             */
            ~TcpSocket() override = default;

            public:
            /**
             * @brief
             * Binds to ip:port, starts listening, and attaches to the reactor
             * watching for incoming connections.
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
             * Notified of each accepted connection. Must outlive this socket.
             *
             * @return
             * True if bind, listen, and the reactor attach all succeeded.
             */
            [[nodiscard]]
            bool listen(std::uint32_t ip,
                        std::uint16_t port,
                        std::string_view reactor_name,
                        ITcpConnectorListener& listener);

            /**
             * @brief
             * Actively connects to ip:port and attaches to the reactor.
             * Completion is asynchronous — the listener's on_connected()
             * fires once the handshake finishes, on_closed() if it fails.
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
             * Notified of connection events and data. Must outlive this socket.
             *
             * @return
             * True if the attempt started and the reactor attach succeeded.
             * Not an indication that the connection is established yet.
             */
            [[nodiscard]]
            bool connect(std::uint32_t ip,
                         std::uint16_t port,
                         std::string_view reactor_name,
                         TcpChannelListener& listener);

            /**
             * @brief
             * Attaches an already-connected fd to the reactor — a connection
             * delivered by ITcpConnectorListener::on_accepted().
             *
             * @param reactor_name
             * Reactor to attach to.
             *
             * @param listener
             * Notified of data and closure. Must outlive this socket.
             *
             * @return
             * True if the reactor attach succeeded.
             */
            [[nodiscard]]
            bool attach(std::string_view reactor_name, TcpChannelListener& listener);

            /**
             * @brief
             * Queues data to be sent, registering interest in writability if
             * the write buffer was empty before this call.
             *
             * @param data
             * Source bytes to send.
             *
             * @param size
             * Number of bytes to send.
             *
             * @return
             * BufferCode::OK on success, BufferCode::ERR_FULL if the write
             * buffer is out of room, BufferCode::NOT_REQUIRED if this socket
             * is not a connection (listening, or never set up).
             */
            [[nodiscard]]
            buffer::BufferCode write(const char* data, std::size_t size) noexcept;

            /**
             * @brief
             * Returns true if there is unsent data in the write buffer.
             */
            [[nodiscard]]
            bool write_pending() const noexcept {
                return !write_buffer_.empty();
            }

            /**
             * @brief
             * Returns true if this socket is listening for connections.
             */
            [[nodiscard]]
            bool is_listening() const noexcept {
                return mode_ == Mode::LISTENING;
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
             * Called by the reactor when the fd is readable. Accepts, checks
             * connect completion, or receives, depending on mode_.
             */
            void on_read() noexcept override;

            /**
             * @brief
             * Called by the reactor when the fd is writable. Completes an
             * outbound connect, then flushes the write buffer.
             */
            void on_write() noexcept override;

            /**
             * @brief
             * Called by the reactor on error or hangup. Tears down.
             */
            void on_error() noexcept override;

            private:
            /**
             * @brief
             * Accepts until EAGAIN, handing each new fd to the connector
             * listener.
             */
            void accept_loop() noexcept;

            /**
             * @brief
             * Receives until EAGAIN, handing buffered bytes to the channel
             * listener and erasing whatever it consumed.
             */
            void receive_loop() noexcept;

            /**
             * @brief
             * Checks whether an in-flight connect() succeeded, promotes the
             * socket to CONNECTED, and fires on_connected().
             *
             * @return
             * True if the connection is now established and still open.
             */
            [[nodiscard]]
            bool finish_connect() noexcept;

            /**
             * @brief
             * Tears down and notifies whichever listener is set.
             */
            void shutdown_and_notify() noexcept;

            /**
             * @brief
             * Sets reactor interest to IN, plus OUT when data is queued.
             */
            void refresh_interest() noexcept;

            private:
            /**
             * @brief
             * What this socket is. Fixed by whichever setup method was called
             * and never crosses over, matching what the kernel permits for
             * the underlying fd.
             */
            enum class Mode : std::uint8_t {
                NONE = 0,
                LISTENING,
                CONNECTING,
                CONNECTED
            };

            private:
            /**
             * @brief
             * Reactor this socket attached to, needed to detach on teardown.
             */
            std::string reactor_name_;

            /**
             * @brief
             * Non-owning. Set only in LISTENING mode.
             */
            ITcpConnectorListener* connector_listener_ = nullptr;

            /**
             * @brief
             * Non-owning. Set only in CONNECTING and CONNECTED modes.
             */
            TcpChannelListener* channel_listener_ = nullptr;

            /**
             * @brief
             * Staging buffer for received bytes. Unused when listening.
             */
            buffer::SocketBuffer read_buffer_;

            /**
             * @brief
             * Staging buffer for bytes queued to send. Unused when listening.
             */
            buffer::SocketBuffer write_buffer_;

            /**
             * @brief
             * Current role and lifecycle state.
             */
            Mode mode_ = Mode::NONE;
        };
    }
}
