#include "log_hub.hpp"

#include "../shared/time_helper.hpp"

namespace impetus {
    namespace log {
        void LogHub::log(std::string_view producer_name,
                         LogLevel level,
                         std::string_view tag,
                         std::string_view message,
                         const char* file,
                         std::int32_t line) {
            std::shared_ptr<ILogProducer> producer;
            {
                std::lock_guard lock(mutex_);
                auto it = producers_.find(std::string(producer_name));
                if (it == producers_.end()) {
                    return;
                }

                producer = it->second;
            }

            LogEntry entry{};
            entry.level = level;
            entry.level_str = to_string(level);
            entry.timestamp = shared::get_full_timestamp();
            entry.tag = tag;
            entry.message = message;
            entry.file = file;
            entry.line = line;

            producer->emit(entry);
        }

        bool LogHub::add_producer(std::string_view name, std::unique_ptr<ILogProducer> producer) {
            if (!producer) {
                return false;
            }

            std::lock_guard lock(mutex_);

            std::string key(name);
            if (producers_.contains(key)) {
                return false;
            }

            producers_.emplace(std::move(key), std::shared_ptr<ILogProducer>(std::move(producer)));
            return true;
        }

        void LogHub::remove_producer(std::string_view name) {
            std::lock_guard lock(mutex_);
            producers_.erase(std::string(name));
        }

        bool LogHub::add_consumer(std::string_view producer_name,
                                  std::unique_ptr<ILogConsumer> consumer) {
            if (!consumer) {
                return false;
            }

            std::lock_guard lock(mutex_);

            auto it = producers_.find(std::string(producer_name));
            if (it == producers_.end()) {
                return false;
            }

            it->second->add_consumer(std::move(consumer));
            return true;
        }
    }
}
