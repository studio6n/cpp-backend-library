#pragma once

#include <cstdint>
#include <sys/epoll.h>

namespace impetus {
    namespace poll {
        /**
         * @brief
         * Strongly-typed wrapper over epoll event flags.
         */
        enum class EventPollFlag : std::uint32_t {
            NONE = 0,
            IN = EPOLLIN,
            OUT = EPOLLOUT,
            ERR = EPOLLERR,
            HUP = EPOLLHUP
        };

        /**
         * @brief
         * Combines two flags with bitwise OR.
         */
        [[nodiscard]]
        constexpr EventPollFlag operator|(EventPollFlag lhs, EventPollFlag rhs) noexcept {
            return static_cast<EventPollFlag>(static_cast<std::uint32_t>(lhs)
                                              | static_cast<std::uint32_t>(rhs));
        }

        /**
         * @brief
         * Masks two flags with bitwise AND.
         */
        [[nodiscard]]
        constexpr EventPollFlag operator&(EventPollFlag lhs, EventPollFlag rhs) noexcept {
            return static_cast<EventPollFlag>(static_cast<std::uint32_t>(lhs)
                                              & static_cast<std::uint32_t>(rhs));
        }

        /**
         * @brief
         * Inverts all bits of a flag.
         */
        [[nodiscard]]
        constexpr EventPollFlag operator~(EventPollFlag flag) noexcept {
            return static_cast<EventPollFlag>(~static_cast<std::uint32_t>(flag));
        }

        /**
         * @brief
         * Combines rhs into lhs with bitwise OR in place.
         */
        constexpr EventPollFlag& operator|=(EventPollFlag& lhs, EventPollFlag rhs) noexcept {
            lhs = lhs | rhs;
            return lhs;
        }

        /**
         * @brief
         * Masks lhs with rhs using bitwise AND in place.
         */
        constexpr EventPollFlag& operator&=(EventPollFlag& lhs, EventPollFlag rhs) noexcept {
            lhs = lhs & rhs;
            return lhs;
        }

        /**
         * @brief
         * Converts an EventPollFlag to its native epoll bitmask value.
         *
         * @param flag
         * The flag to convert.
         *
         * @return
         * The native epoll bitmask.
         */
        [[nodiscard]]
        constexpr std::uint32_t to_epoll_native(EventPollFlag flag) noexcept {
            return static_cast<std::uint32_t>(flag);
        }
    }
}
