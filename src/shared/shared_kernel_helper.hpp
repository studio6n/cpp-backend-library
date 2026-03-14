#pragma once

#include <sstream>

namespace impetus {
    namespace shared {
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
        bool fcntl_non_blocking(int fd, std::ostringstream* err_out = nullptr);

        /**
         * @brief
         * Closes an fd without diagnostics.
         * @param fd
         * The fd to close
         * @return
         * True if close succeeded, false otherwise.
         */
        bool fd_close(int fd) noexcept;

        /**
         * @brief
         * Closes an fd with diagnostics.
         * @param fd
         * The fd to close
         * @param err_out
         * The error output stream for diagnostics.
         * @return
         * True if close succeeded, false otherwise.
         */
        bool fd_close(int fd, std::ostringstream& err_out) noexcept;
    }
}