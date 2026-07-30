#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "log_level.hpp"

namespace impetus {
    namespace log {
        /**
         * @brief
         * A single log record, fully populated before being dispatched to consumers.
         *
         * level_str and timestamp are precomputed by LogHub::log() so that
         * ILogConsumer implementations perform no formatting-time conversion work.
         */
        struct LogEntry {
            /**
             * @brief
             * Severity level of this entry.
             */
            LogLevel level;

            /**
             * @brief
             * String representation of level, precomputed via to_string().
             */
            const char* level_str;

            /**
             * @brief
             * Local time the entry was created, formatted as HH-MM-SS:sss.
             */
            std::string timestamp;

            /**
             * @brief
             * Short identifier for the source component.
             */
            std::string_view tag;

            /**
             * @brief
             * Human-readable log message.
             */
            std::string_view message;

            /**
             * @brief
             * Source file, from __FILE__.
             */
            const char* file;

            /**
             * @brief
             * Source line, from __LINE__.
             */
            std::int32_t line;
        };
    }
}
