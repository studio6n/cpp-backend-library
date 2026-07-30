#pragma once

#include <cstdint>

namespace impetus {
    namespace log {
        /**
         * @brief
         * Severity level for a log entry.
         */
        enum class LogLevel : std::uint8_t {
            DEBUG = 0,
            INFO,
            WARN,
            ERROR,
            FATAL
        };

        /**
         * @brief
         * Converts a LogLevel to its string representation.
         *
         * @param level
         * The level to convert.
         *
         * @return
         * Static string literal naming the level. "UNKNOWN" if unrecognised.
         */
        [[nodiscard]]
        constexpr const char* to_string(LogLevel level) noexcept {
            switch (level) {
                case LogLevel::DEBUG:
                    return "DEBUG";
                case LogLevel::INFO:
                    return "INFO";
                case LogLevel::WARN:
                    return "WARN";
                case LogLevel::ERROR:
                    return "ERROR";
                case LogLevel::FATAL:
                    return "FATAL";
            }

            return "UNKNOWN";
        }
    }
}
