#include "base_socket.hpp"

#include <fcntl.h>
#include <unistd.h>

#include "../poll/event_poll_registry.hpp"

namespace impetus {
    namespace network {
        BaseSocket::BaseSocket(int fd) noexcept
            : poll::EventPollSubscriber(fd) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                // Every read/write path in this module relies on EAGAIN to
                // terminate its loop — a blocking fd would stall the whole
                // reactor thread on a single slow peer.
                ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
        }

        BaseSocket::~BaseSocket() {
            if (has_valid_fd()) {
                ::close(fd());
                invalidate_fd();
            }
        }

        void BaseSocket::begin_shutdown(std::string_view reactor_name) noexcept {
            if (closing_) {
                return;
            }

            closing_ = true;

            if (!has_valid_fd()) {
                return;
            }

            if (!reactor_name.empty()) {
                // Must run before invalidate_fd() — detach() reads fd() to
                // locate its entry in the reactor.
                poll::EventPollRegistry::instance().detach(reactor_name, *this);
            }

            ::close(fd());
            invalidate_fd();
        }
    }
}
