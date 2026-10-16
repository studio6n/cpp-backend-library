#include "udp_socket.hpp"

#include <cerrno>
#include <netinet/in.h>

#include "../poll/event_poll_flag.hpp"
#include "i_udp_listener.hpp"

namespace impetus {
    namespace network {
        UdpSocket::UdpSocket(int fd, BufferConfig read_config) noexcept
            : BaseSocket(fd)
            , read_buffer_(read_config.memory, read_config.capacity)
            , sender_addr_{}
            , sender_len_(static_cast<socklen_t>(sizeof(sockaddr_storage))) {
        }

        bool UdpSocket::bind(std::uint32_t ip,
                             std::uint16_t port,
                             std::string_view reactor_name,
                             IUdpListener& listener) {
            if (listener_ != nullptr || !has_valid_fd()) {
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

            return attach(reactor_name, listener);
        }

        bool UdpSocket::connect(std::uint32_t ip,
                                std::uint16_t port,
                                std::string_view reactor_name,
                                IUdpListener& listener) {
            if (listener_ != nullptr || !has_valid_fd()) {
                return false;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = ip;
            address.sin_port = htons(port);

            // UDP connect is a local filter, not a handshake — it resolves
            // immediately, so there is no EINPROGRESS case to wait on.
            if (::connect(fd(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
                return false;
            }

            if (!attach(reactor_name, listener)) {
                return false;
            }

            connected_ = true;
            return true;
        }

        bool UdpSocket::write(const char* data,
                              std::size_t size,
                              const sockaddr* destination,
                              socklen_t destination_len) noexcept {
            if (!has_valid_fd()) {
                return false;
            }

            const ssize_t sent
                = ::sendto(fd(), data, size, MSG_NOSIGNAL, destination, destination_len);
            return sent >= 0 && static_cast<std::size_t>(sent) == size;
        }

        bool UdpSocket::write(const char* data, std::size_t size) noexcept {
            if (!has_valid_fd() || !connected_) {
                return false;
            }

            const ssize_t sent = ::send(fd(), data, size, MSG_NOSIGNAL);
            return sent >= 0 && static_cast<std::size_t>(sent) == size;
        }

        void UdpSocket::close() noexcept {
            shutdown_and_notify();
        }

        void UdpSocket::on_read() noexcept {
            while (has_valid_fd()) {
                read_buffer_.clear();

                const std::size_t space = read_buffer_.free_space();
                if (space == 0) {
                    return;
                }

                sender_len_ = static_cast<socklen_t>(sizeof(sockaddr_storage));

                // recvfrom even when connected: the kernel only delivers from
                // the connected peer anyway, so sender_addr_ stays correct and
                // there is one receive path instead of two.
                const ssize_t received = ::recvfrom(fd(),
                                                    read_buffer_.write_ptr(),
                                                    space,
                                                    0,
                                                    reinterpret_cast<sockaddr*>(&sender_addr_),
                                                    &sender_len_);
                if (received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    shutdown_and_notify();
                    return;
                }

                read_buffer_.commit(static_cast<std::size_t>(received));

                if (listener_ != nullptr) {
                    listener_->on_datagram(
                        read_buffer_.data(), read_buffer_.size(), sender_addr_, sender_len_);
                }

                // on_datagram() may have closed this socket.
                if (!has_valid_fd()) {
                    return;
                }
            }
        }

        void UdpSocket::on_error() noexcept {
            shutdown_and_notify();
        }

        bool UdpSocket::attach(std::string_view reactor_name, IUdpListener& listener) {
            listener_ = &listener;

            if (!attach_to_reactor(reactor_name, poll::EventPollFlag::IN)) {
                listener_ = nullptr;
                return false;
            }

            return true;
        }

        void UdpSocket::shutdown_and_notify() noexcept {
            if (is_closing()) {
                return;
            }

            begin_shutdown();

            // Notify last — the listener may react by tearing down whatever
            // owns this socket, so nothing may touch members after this.
            if (listener_ != nullptr) {
                listener_->on_closed();
            }
        }
    }
}
