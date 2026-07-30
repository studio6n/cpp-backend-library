#pragma once

#include <sstream>

namespace impetus {
    namespace shared {
        /**
         * @brief
         * Sets a file descriptor to non-blocking mode.
         *
         * @param fd
         * The file descriptor to modify.
         *
         * @param err_out
         * Optional diagnostics sink. Appended on error if provided.
         *
         * @return
         * True on success, false on failure.
         */
        [[nodiscard]]
        bool fcntl_non_blocking(int fd, std::ostringstream* err_out = nullptr);

        /**
         * @brief
         * Closes a file descriptor.
         *
         * @param fd
         * The file descriptor to close.
         *
         * @return
         * True if close succeeded, false otherwise.
         */
        [[nodiscard]]
        bool fd_close(int fd) noexcept;

        /**
         * @brief
         * Closes a file descriptor with diagnostics on failure.
         *
         * @param fd
         * The file descriptor to close.
         *
         * @param err_out
         * Receives a diagnostic message on failure.
         *
         * @return
         * True if close succeeded, false otherwise.
         */
        [[nodiscard]]
        bool fd_close(int fd, std::ostringstream& err_out) noexcept;
    }
}
