#pragma once

#include "log_hub.hpp"
#include "log_level.hpp"

/**
 * LOG(producer_name, level, tag, message)
 *
 * Routes a log entry to the named producer registered with LogHub.
 * __FILE__ and __LINE__ are injected automatically.
 *
 * @param producer_name  String handle of the target producer.
 * @param level          impetus::log::LogLevel value (DEBUG, INFO, WARN, ERROR, FATAL).
 * @param tag            String literal identifying the source component.
 * @param message        String literal or std::string_view with the log message.
 *
 * Naming note: LOG has no module prefix — accepted explicitly, deviates from
 * the LOG_ prefix convention for logging macros. Risk: may clash with a
 * third-party macro also named LOG.
 */
#define LOG(producer_name, level, tag, message)                                                    \
    ::impetus::log::LogHub::instance().log(                                                        \
        (producer_name), (level), (tag), (message), __FILE__, __LINE__)
