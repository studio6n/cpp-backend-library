#pragma once

#include <cstdint>
#include <limits>

namespace impetus {
    namespace buffer {
        /**
         * @brief
         * Return codes for mutating buffer operations.
         */
        enum class BufferCode : std::size_t {
            OK = 0, // Operation succeeded.
            NOT_REQUIRED = 1, // Operation was a no-op (already satisfied).
            ERR_FULL = 2, // Not enough free space.
            ERR_ALLOC = 3 // Initial memory allocation failed.
        };

        /**
         * @brief
         * Sentinel value representing end-of-buffer or not-found.
         * Equivalent to std::string::npos.
         */
        constexpr std::size_t K_BUFFER_NPOS = std::numeric_limits<std::size_t>::max();
    }
}
