#include "base_socket.hpp"

#include <fcntl.h>
#include <unistd.h>

#include "../log/logger.hpp"
#include "../poll/event_poll_registry.hpp"
#include "../shared/constants.hpp"

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
            if (!closing_ && !reactor_name_.empty()) {
                LOG(shared::CORE_LOG_PRODUCER,
                    log::LogLevel::ERROR,
                    "network",
                    "socket destroyed while attached, without close(); shutting it down");
            }

            begin_shutdown();
        }

        bool BaseSocket::attach_to_reactor(std::string_view reactor_name,
                                           poll::EventPollFlag interest) {
            if (closing_ || !reactor_name_.empty() || !has_valid_fd()) {
                return false;
            }

            // Recorded before the attach: once queued, the reactor thread can
            // call back into this socket, and those callbacks read reactor_name_.
            reactor_name_ = std::string(reactor_name);

            if (!poll::EventPollRegistry::instance().attach(
                    reactor_name_, interest, weak_from_this())) {
                reactor_name_.clear();
                return false;
            }

            return true;
        }

        void BaseSocket::begin_shutdown() noexcept {
            if (closing_) {
                return;
            }

            closing_ = true;

            if (!has_valid_fd()) {
                return;
            }

            if (reactor_name_.empty()) {
                // No reactor ever accepted this fd, so nothing else knows the number.
                ::close(fd());
            } else {
                // The reactor closes the fd after removing it; closing it here
                // would free the number while the reactor still has an entry for
                // it. Must run before invalidate_fd() — detach() reads fd().
                poll::EventPollRegistry::instance().detach(reactor_name_, *this);
            }

            invalidate_fd();
        }
    }
}
