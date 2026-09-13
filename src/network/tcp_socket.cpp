#include "tcp_socket.hpp"

#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>

#include "../poll/event_poll_flag.hpp"
#include "../poll/event_poll_registry.hpp"
#include "i_tcp_connector_listener.hpp"
#include "tcp_channel_listener.hpp"

namespace impetus {
    namespace network {
        TcpSocket::TcpSocket(int fd, BufferConfig read_config, BufferConfig write_config) noexcept
            : BaseSocket(fd)
            , read_buffer_(read_config.memory, read_config.capacity)
            , write_buffer_(write_config.memory, write_config.capacity) {
        }

        bool TcpSocket::listen(std::uint32_t ip,
                               std::uint16_t port,
                               std::string_view reactor_name,
                               ITcpConnectorListener& listener) {
            if (mode_ != Mode::NONE || !has_valid_fd()) {
                return false;
            }

            const int option = 1;
            if (::setsockopt(fd(), SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
                return false;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = ip;
            address.sin_port = htons(port);

            if (::bind(fd(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
                return false;
            }

            if (::listen(fd(), SOMAXCONN) < 0) {
                return false;
            }

            reactor_name_ = std::string(reactor_name);
            connector_listener_ = &listener;
            mode_ = Mode::LISTENING;

            if (!poll::EventPollRegistry::instance().attach(
                    reactor_name_, poll::EventPollFlag::IN, this, this)) {
                mode_ = Mode::NONE;
                connector_listener_ = nullptr;
                return false;
            }

            return true;
        }

        bool TcpSocket::connect(std::uint32_t ip,
                                std::uint16_t port,
                                std::string_view reactor_name,
                                TcpChannelListener& listener) {
            if (mode_ != Mode::NONE || !has_valid_fd()) {
                return false;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = ip;
            address.sin_port = htons(port);

            reactor_name_ = std::string(reactor_name);
            channel_listener_ = &listener;
            mode_ = Mode::CONNECTING;

            if (::connect(fd(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
                // The fd is non-blocking, so the handshake almost always
                // reports EINPROGRESS and completes later via EPOLLOUT.
                if (errno != EINPROGRESS) {
                    mode_ = Mode::NONE;
                    channel_listener_ = nullptr;
                    return false;
                }
            } else {
                mode_ = Mode::CONNECTED;
            }

            if (!poll::EventPollRegistry::instance().attach(reactor_name_,
                                                            poll::EventPollFlag::IN
                                                                | poll::EventPollFlag::OUT,
                                                            this,
                                                            this)) {
                mode_ = Mode::NONE;
                channel_listener_ = nullptr;
                return false;
            }

            return true;
        }

        bool TcpSocket::attach(std::string_view reactor_name, TcpChannelListener& listener) {
            if (mode_ != Mode::NONE || !has_valid_fd()) {
                return false;
            }

            reactor_name_ = std::string(reactor_name);
            channel_listener_ = &listener;
            mode_ = Mode::CONNECTED;

            if (!poll::EventPollRegistry::instance().attach(
                    reactor_name_, poll::EventPollFlag::IN, this, this)) {
                mode_ = Mode::NONE;
                channel_listener_ = nullptr;
                return false;
            }

            return true;
        }

        buffer::BufferCode TcpSocket::write(const char* data, std::size_t size) noexcept {
            if (mode_ != Mode::CONNECTED && mode_ != Mode::CONNECTING) {
                return buffer::BufferCode::NOT_REQUIRED;
            }

            if (!has_valid_fd()) {
                return buffer::BufferCode::NOT_REQUIRED;
            }

            const bool was_empty = write_buffer_.empty();
            const buffer::BufferCode result = write_buffer_.append(data, size);

            if (result == buffer::BufferCode::OK && was_empty && !write_buffer_.empty()) {
                refresh_interest();
            }

            return result;
        }

        void TcpSocket::close() noexcept {
            shutdown_and_notify();
        }

        void TcpSocket::on_read() noexcept {
            switch (mode_) {
                case Mode::LISTENING:
                    accept_loop();
                    break;

                case Mode::CONNECTING:
                    // Readability before the handshake completes signals
                    // failure, not data. finish_connect() confirms via
                    // SO_ERROR and tears down if it failed.
                    if (finish_connect()) {
                        receive_loop();
                    }
                    break;

                case Mode::CONNECTED:
                    receive_loop();
                    break;

                case Mode::NONE:
                    break;
            }
        }

        void TcpSocket::on_write() noexcept {
            if (mode_ == Mode::CONNECTING && !finish_connect()) {
                return;
            }

            if (mode_ != Mode::CONNECTED || !has_valid_fd()) {
                return;
            }

            while (!write_buffer_.empty()) {
                const ssize_t sent
                    = ::send(fd(), write_buffer_.data(), write_buffer_.size(), MSG_NOSIGNAL);
                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    shutdown_and_notify();
                    return;
                }

                write_buffer_.erase(0, static_cast<std::size_t>(sent));
            }

            // Drained — stop asking the reactor about writability, otherwise
            // it fires on every cycle for a socket with nothing to send.
            refresh_interest();

            if (channel_listener_ != nullptr) {
                channel_listener_->on_writable();
            }
        }

        void TcpSocket::on_error() noexcept {
            shutdown_and_notify();
        }

        void TcpSocket::accept_loop() noexcept {
            while (has_valid_fd()) {
                sockaddr_storage peer{};
                socklen_t peer_len = static_cast<socklen_t>(sizeof(peer));

                const int accepted = ::accept(fd(), reinterpret_cast<sockaddr*>(&peer), &peer_len);
                if (accepted < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    shutdown_and_notify();
                    return;
                }

                if (connector_listener_ == nullptr) {
                    ::close(accepted);
                    continue;
                }

                connector_listener_->on_accepted(accepted, peer, peer_len);

                // on_accepted() may have closed this listening socket.
                if (!has_valid_fd()) {
                    return;
                }
            }
        }

        void TcpSocket::receive_loop() noexcept {
            while (has_valid_fd()) {
                const std::size_t space = read_buffer_.free_space();
                if (space == 0) {
                    // The listener is not consuming and the buffer is full;
                    // nothing more can be read until it does.
                    return;
                }

                const ssize_t received = ::recv(fd(), read_buffer_.write_ptr(), space, 0);
                if (received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    shutdown_and_notify();
                    return;
                }

                if (received == 0) {
                    // Peer closed the connection.
                    shutdown_and_notify();
                    return;
                }

                read_buffer_.commit(static_cast<std::size_t>(received));

                if (channel_listener_ == nullptr) {
                    read_buffer_.clear();
                    continue;
                }

                const std::size_t buffered = read_buffer_.size();
                std::size_t consumed = channel_listener_->on_data(read_buffer_.data(), buffered);

                // on_data() may have closed this socket.
                if (!has_valid_fd()) {
                    return;
                }

                if (consumed > buffered) {
                    consumed = buffered;
                }

                if (consumed > 0) {
                    read_buffer_.erase(0, consumed);
                }
            }
        }

        bool TcpSocket::finish_connect() noexcept {
            int error = 0;
            socklen_t length = static_cast<socklen_t>(sizeof(error));

            if (::getsockopt(fd(), SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) {
                shutdown_and_notify();
                return false;
            }

            mode_ = Mode::CONNECTED;
            refresh_interest();

            if (channel_listener_ != nullptr) {
                channel_listener_->on_connected();
            }

            return has_valid_fd();
        }

        void TcpSocket::shutdown_and_notify() noexcept {
            if (is_closing()) {
                return;
            }

            begin_shutdown(reactor_name_);

            // Notify last — the listener may react by tearing down whatever
            // owns this socket, so nothing may touch members after this.
            if (connector_listener_ != nullptr) {
                connector_listener_->on_closed();
                return;
            }

            if (channel_listener_ != nullptr) {
                channel_listener_->on_closed();
            }
        }

        void TcpSocket::refresh_interest() noexcept {
            if (!has_valid_fd() || reactor_name_.empty()) {
                return;
            }

            const poll::EventPollFlag interest
                = write_buffer_.empty() ? poll::EventPollFlag::IN
                                        : (poll::EventPollFlag::IN | poll::EventPollFlag::OUT);

            poll::EventPollRegistry::instance().modify(reactor_name_, fd(), interest);
        }
    }
}
