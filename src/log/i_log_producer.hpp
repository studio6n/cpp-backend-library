#pragma once

#include "../dispatch/i_producer.hpp"
#include "log_entry.hpp"

namespace impetus {
    namespace log {
        /**
         * @brief
         * Producer of LogEntry records. Registered with LogHub under a named
         * handle. Fans out dispatched entries to its own registered ILogConsumers.
         */
        class ILogProducer : public dispatch::IProducer<LogEntry> {
            public:
            /**
             * @brief
             * Fans out a log entry to this producer's registered consumers.
             * Public passthrough to the protected IProducer<LogEntry>::dispatch() —
             * LogHub routes entries to a named producer from outside the class
             * hierarchy, so it cannot call the protected member directly.
             *
             * @param entry
             * The log entry to fan out.
             */
            void emit(const LogEntry& entry) noexcept {
                dispatch(entry);
            }

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            ILogProducer() = default;
        };
    }
}
