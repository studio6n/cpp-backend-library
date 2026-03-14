#pragma once

#include <cstdint>
#include <sys/epoll.h>

namespace impetus {
    namespace shared {
        /**
         * Wrapper enum class for event poll flags.
         */
        enum class EventPollFlag : std::uint32_t {
            NONE = 0,
            IN = EPOLLIN,
            OUT = EPOLLOUT,
            ERR = EPOLLERR,
            HUP = EPOLLHUP
        };

        [[nodiscard]]
        constexpr EventPollFlag operator|(EventPollFlag lhs, EventPollFlag rhs) noexcept {
            return static_cast<EventPollFlag>(static_cast<std::uint32_t>(lhs) |
                                              static_cast<std::uint32_t>(rhs));
        }

        [[nodiscard]]
        constexpr EventPollFlag operator&(EventPollFlag lhs, EventPollFlag rhs) noexcept {
            return static_cast<EventPollFlag>(static_cast<std::uint32_t>(lhs) &
                                              static_cast<std::uint32_t>(rhs));
        }

        [[nodiscard]]
        constexpr EventPollFlag operator~(EventPollFlag _flag) noexcept {
            return static_cast<EventPollFlag>(~static_cast<std::uint32_t>(_flag));
        }

        [[nodiscard]]
        constexpr EventPollFlag operator|=(EventPollFlag& lhs, EventPollFlag rhs) noexcept {
            lhs = lhs | rhs;
            return lhs;
        }

        [[nodiscard]]
        constexpr EventPollFlag operator&=(EventPollFlag& lhs, EventPollFlag rhs) noexcept {
            lhs = lhs & rhs;
            return lhs;
        }

        /**
         * Converts EventPollFlag to have event poll's flag.
         * @param flag
         * The enum value to convert to.
         */
        [[nodiscard]]
        constexpr std::uint32_t to_epoll_native(EventPollFlag flag) noexcept {
            return static_cast<std::uint32_t>(flag);
        }
    }
}