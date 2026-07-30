#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "i_log_consumer.hpp"
#include "i_log_producer.hpp"
#include "log_level.hpp"

namespace impetus {
    namespace log {
        /**
         * @brief
         * Registry of named log producers. Routes each LOG call to the producer
         * identified by its handle. Each producer fans out to its own registered
         * consumers — this is what allows independent, simultaneous log files.
         *
         * Thread-safe. Not copyable or movable. Meyers singleton.
         */
        class LogHub {
            public:
            /**
             * @brief
             * Returns the global LogHub instance.
             */
            [[nodiscard]]
            static LogHub& instance() {
                static LogHub hub;
                return hub;
            }

            /**
             * @brief
             * Copy construction is disabled.
             */
            LogHub(const LogHub&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            LogHub& operator=(const LogHub&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            LogHub(LogHub&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            LogHub& operator=(LogHub&&) = delete;

            public:
            /**
             * @brief
             * Routes a log entry to the named producer. No-op if the producer
             * is not registered. Called by the LOG macro — do not call directly.
             *
             * @param producer_name
             * Named handle of the target producer.
             *
             * @param level
             * Severity level.
             *
             * @param tag
             * Short identifier for the source component.
             *
             * @param message
             * Human-readable log message.
             *
             * @param file
             * Source file, from __FILE__.
             *
             * @param line
             * Source line, from __LINE__.
             */
            void log(std::string_view producer_name,
                     LogLevel level,
                     std::string_view tag,
                     std::string_view message,
                     const char* file,
                     std::int32_t line);

            /**
             * @brief
             * Takes ownership of a producer and registers it under a named handle.
             *
             * @param name
             * Unique handle for this producer.
             *
             * @param producer
             * Ownership transferred to LogHub.
             *
             * @return
             * True if registered. False if the name already exists or producer is null.
             */
            [[nodiscard]]
            bool add_producer(std::string_view name, std::unique_ptr<ILogProducer> producer);

            /**
             * @brief
             * Removes the producer registered under name. Destroys it — and,
             * since the producer owns its consumers, everything registered
             * with it — once every in-flight log() call using it completes.
             *
             * @param name
             * Handle of the producer to remove.
             */
            void remove_producer(std::string_view name);

            /**
             * @brief
             * Takes ownership of a consumer and registers it with the named
             * producer. The producer owns the consumer from this point on —
             * LogHub keeps no separate reference to it.
             *
             * @param producer_name
             * Handle of the target producer.
             *
             * @param consumer
             * Ownership transferred to the producer.
             *
             * @return
             * True if registered. False if the producer is not found or consumer is null.
             */
            [[nodiscard]]
            bool add_consumer(std::string_view producer_name,
                              std::unique_ptr<ILogConsumer> consumer);

            private:
            /**
             * @brief
             * Default constructor. Only accessible via instance().
             */
            LogHub() = default;

            private:
            /**
             * @brief
             * Guards producers_.
             */
            std::mutex mutex_;

            /**
             * @brief
             * Producers keyed by name. shared_ptr, not unique_ptr: log() copies
             * the pointer out while holding mutex_, then releases the lock
             * before calling emit() on it. A unique pointer would leave that
             * copy dangling if remove_producer() erased the entry concurrently —
             * shared_ptr keeps the producer alive until every in-flight log()
             * call finishes with it. Each producer owns its own consumers
             * (see IProducer<T>), so no separate bookkeeping is needed here.
             */
            std::unordered_map<std::string, std::shared_ptr<ILogProducer>> producers_;
        };
    }
}
