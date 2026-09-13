#include "tcp_channel_listener.hpp"

#include <unistd.h>

#include "tcp_socket.hpp"

namespace impetus {
    namespace network {
        TcpChannelListener::~TcpChannelListener() = default;

        bool TcpChannelListener::adopt(int fd,
                                       std::string_view reactor_name,
                                       BufferConfig read_config,
                                       BufferConfig write_config) {
            if (socket_) {
                return false;
            }

            socket_ = std::make_unique<TcpSocket>(fd, read_config, write_config);
            if (!socket_->attach(reactor_name, *this)) {
                // The socket's destructor closes the fd.
                socket_.reset();
                return false;
            }

            return true;
        }

        bool TcpChannelListener::connect(int fd,
                                         std::string_view reactor_name,
                                         std::uint32_t ip,
                                         std::uint16_t port,
                                         BufferConfig read_config,
                                         BufferConfig write_config) {
            if (socket_) {
                return false;
            }

            socket_ = std::make_unique<TcpSocket>(fd, read_config, write_config);
            if (!socket_->connect(ip, port, reactor_name, *this)) {
                socket_.reset();
                return false;
            }

            return true;
        }

        buffer::BufferCode TcpChannelListener::write(const char* data, std::size_t size) noexcept {
            if (!socket_) {
                return buffer::BufferCode::NOT_REQUIRED;
            }

            return socket_->write(data, size);
        }

        int TcpChannelListener::fd() const noexcept {
            if (!socket_) {
                return poll::EventPollSubscriber::K_INVALID_FD;
            }

            return socket_->fd();
        }

        bool TcpChannelListener::is_open() const noexcept {
            return socket_ && socket_->has_valid_fd();
        }

        void TcpChannelListener::close() noexcept {
            if (!socket_) {
                return;
            }

            socket_->close();
        }
    }
}
