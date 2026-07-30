#pragma once

#include <bit>
#include <cstdint>

namespace impetus {
    namespace shared {
        /**
         * @brief
         * Converts a 16-bit value from host byte order to network byte order.
         *
         * @param value
         * The value to convert.
         *
         * @return
         * The converted value in network byte order.
         */
        [[nodiscard]]
        constexpr std::uint16_t host_to_net16(std::uint16_t value) noexcept {
            if constexpr (std::endian::native == std::endian::little) {
                return __builtin_bswap16(value);
            } else {
                return value;
            }
        }

        /**
         * @brief
         * Converts a 32-bit value from host byte order to network byte order.
         *
         * @param value
         * The value to convert.
         *
         * @return
         * The converted value in network byte order.
         */
        [[nodiscard]]
        constexpr std::uint32_t host_to_net32(std::uint32_t value) noexcept {
            if constexpr (std::endian::native == std::endian::little) {
                return __builtin_bswap32(value);
            } else {
                return value;
            }
        }

        /**
         * @brief
         * Converts a 64-bit value from host byte order to network byte order.
         *
         * @param value
         * The value to convert.
         *
         * @return
         * The converted value in network byte order.
         */
        [[nodiscard]]
        constexpr std::uint64_t host_to_net64(std::uint64_t value) noexcept {
            if constexpr (std::endian::native == std::endian::little) {
                return __builtin_bswap64(value);
            } else {
                return value;
            }
        }
    }
}
