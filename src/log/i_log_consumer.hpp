#pragma once

#include "../dispatch/i_consumer.hpp"
#include "log_entry.hpp"

namespace impetus {
    namespace log {
        /**
         * @brief
         * Consumer of LogEntry records. Concrete implementations decide where
         * entries end up — file, console, network, etc.
         */
        class ILogConsumer : public dispatch::IConsumer<LogEntry> {
            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            ILogConsumer() = default;
        };
    }
}
