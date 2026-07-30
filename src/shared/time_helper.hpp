#pragma once

#include <string>

namespace impetus {
    namespace shared {
        /**
         * @brief
         * Returns the current local time formatted as HH-MM-SS:sss.
         * No surrounding brackets — callers apply their own formatting.
         */
        [[nodiscard]]
        std::string get_full_timestamp() noexcept;
    }
}
