#include "shared_kernel_helper.hpp"

#include <cerrno>  // errno
#include <cstring> // strerror_r
#include <fcntl.h> // fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <string>
#include <string_view>
#include <unistd.h> // close

namespace impetus {
    namespace shared {
        std::string errno_to_string(int err) {
            char buf[256]{};
#if defined(_GNU_SOURCE) && !defined(__APPLE__)
            // GNU variant: may return pointer to static string or buf
            char* msg = ::strerror_r(err, buf, sizeof(buf));
            return msg ? std::string(msg) : std::string("Unknown error");
#else
            // XSI variant
            if (::strerror_r(err, buf, sizeof(buf)) == 0)
                return std::string(buf);

            return "Unknown error";
#endif
        }

        void append_errno(std::ostringstream& out, std::string_view prefix, int fd, int err) {
            out << prefix << " fd: " << fd << ". Error: " << errno_to_string(err);
        }

        /**
         * @brief
         * Set non-blocking operation for a field descriptor (fd).
         * @param fd
         * The fd to manipulate
         * @param err_out
         * Optional diagnostics sink. If provided, it is appended on error.
         * @return
         * True on success, false on failure.
         */
        bool fcntl_non_blocking(int fd, std::ostringstream* err_out) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags == -1) {
                const int error = errno;
                if (err_out)
                    append_errno(*err_out, "fcntl(F_GETFL) failed!", fd, error);

                return false;
            }

            if (flags & O_NONBLOCK)
                return true;

            if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                const int error = errno;
                if (err_out)
                    append_errno(*err_out, "fcntl(F_SETFL) failed!", fd, error);

                return false;
            }

            return true;
        }

        bool fd_close(int fd) noexcept {
            return (::close(fd) == 0);
        }

        /**
         * @brief
         * Closes an fd with diagnostics.
         * @param _fd
         * The fd to close
         * @param _err_out
         * The error output stream for diagnostics.
         * @return
         * True if close succeeded, false otherwise.
         */
        bool fd_close(int fd, std::ostringstream& err_out) noexcept {
            if (fd_close(fd))
                return true;

            const int error = errno;
            switch (error) {
            case EBADF:
                append_errno(err_out, "Invalid file descriptor detected!", fd, error);
                break;
            case EINTR:
                append_errno(err_out, "close() was interrupted by a signal!", fd, error);
                break;
            case EIO:
                append_errno(err_out, "An I/O error occurred during close()!", fd, error);
                break;
            case ENOSPC:
                [[fallthrough]];
                break;
            case EDQUOT:
                append_errno(err_out, "No space/quota issue occurred during close()!", fd, error);
                break;
            default:
                append_errno(err_out, "close() failed with an unknown errno!", fd, error);
                break;
            }

            return false;
        }
    }
}