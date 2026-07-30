#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include "i_log_consumer.hpp"

namespace impetus {
    namespace log {
        /**
         * @brief
         * Concrete log consumer. Formats each entry and appends it to a file.
         *
         * Format: [timestamp]\t[LEVEL]\t[TAG]\t[filename (line)]: message
         *
         * The file is opened on construction in append mode and closed on
         * destruction. Not copyable or movable.
         *
         * Thread-safe: consume() may be called concurrently. A registered
         * consumer receives one consume() call per dispatch(), on whichever
         * thread called dispatch() — the same consumer can be entered by
         * multiple threads at once if multiple producers (or one producer
         * called from multiple threads) target it, so file_ writes are
         * serialised internally.
         */
        class LogConsumer : public ILogConsumer {
            public:
            /**
             * @brief
             * Default construction is disabled — output_path is a hard requirement.
             */
            LogConsumer() = delete;

            /**
             * @brief
             * Opens output_path in append mode for writing formatted log lines.
             *
             * @param output_path
             * Path to the log file. Created if it does not already exist.
             */
            explicit LogConsumer(std::string_view output_path);

            /**
             * @brief
             * Copy construction is disabled.
             */
            LogConsumer(const LogConsumer&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            LogConsumer& operator=(const LogConsumer&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            LogConsumer(LogConsumer&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            LogConsumer& operator=(LogConsumer&&) = delete;

            /**
             * @brief
             * Closes the output file.
             */
            ~LogConsumer() = default;

            public:
            /**
             * @brief
             * Formats entry and appends it to the output file.
             *
             * @param entry
             * The log entry to write.
             */
            void consume(const LogEntry& entry) noexcept override;

            private:
            /**
             * @brief
             * Strips the directory portion from a __FILE__ path, keeping the
             * filename and its extension.
             *
             * @param file
             * Full source file path, typically from __FILE__.
             *
             * @return
             * The filename with extension, no path.
             */
            [[nodiscard]]
            static std::string extract_file_name(const char* file);

            private:
            /**
             * @brief
             * Guards file_ against concurrent writes from multiple threads.
             */
            std::mutex file_mutex_;

            /**
             * @brief
             * Output file stream, opened in append mode.
             */
            std::ofstream file_;
        };
    }
}
