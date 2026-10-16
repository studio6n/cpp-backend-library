#pragma once

#include <cstddef>

namespace impetus {
    namespace shared {
        /**
         * @brief
         * Neutral std::size_t value (zero). Used as a baseline for size comparisons.
         */
        constexpr std::size_t K_SIZE_T_NEUTRAL = 0;

        /**
         * @brief
         * LogHub producer name the library modules log under. Messages are
         * dropped until the application registers a producer with this name.
         */
        constexpr const char* CORE_LOG_PRODUCER = "core";
    }
}
