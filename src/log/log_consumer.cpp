#include "log_consumer.hpp"

namespace impetus {
    namespace log {
        LogConsumer::LogConsumer(std::string_view output_path)
            : file_(std::string(output_path), std::ios::app) {
        }

        void LogConsumer::consume(const LogEntry& entry) noexcept {
            try {
                std::lock_guard lock(file_mutex_);
                file_ << '[' << entry.timestamp << "]\t[" << entry.level_str << "]\t[" << entry.tag
                      << "]\t[" << extract_file_name(entry.file) << " (" << entry.line
                      << ")]: " << entry.message << '\n';
                file_.flush();
            } catch (...) {
            }
        }

        std::string LogConsumer::extract_file_name(const char* file) {
            const std::string_view sv(file);
            const auto slash = sv.rfind('/');
            const auto start = (slash == std::string_view::npos) ? 0 : slash + 1;
            return std::string(sv.substr(start));
        }
    }
}
