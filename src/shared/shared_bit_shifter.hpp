#pragma once

#include <bit>
#include <cstdint>

namespace impetus {
    namespace shared {
        constexpr uint16_t xhtons(uint16_t value) noexcept {
            if constexpr (std::endian::native == std::endian::little)
                return __builtin_bswap16(value);
            else
                return value;
        }

        constexpr uint32_t xhtonl(uint32_t value) noexcept {
            if constexpr (std::endian::native == std::endian::little)
                return __builtin_bswap32(value);
            else
                return value;
        }

        constexpr uint64_t xhtonll(uint64_t value) noexcept {
            if constexpr (std::endian::native == std::endian::little)
                return __builtin_bswap64(value);
            else
                return value;
        }
    }
}