#include "time_helper.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace impetus {
    namespace shared {
        std::string get_full_timestamp() noexcept {
            const auto now = std::chrono::system_clock::now();
            const auto time_t = std::chrono::system_clock::to_time_t(now);
            const auto ms
                = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                  % 1000;

            std::tm local_tm{};
            ::localtime_r(&time_t, &local_tm);

            char buf[16];
            std::snprintf(buf,
                          sizeof(buf),
                          "%02d-%02d-%02d:%03lld",
                          local_tm.tm_hour,
                          local_tm.tm_min,
                          local_tm.tm_sec,
                          static_cast<long long>(ms.count()));

            return buf;
        }
    }
}
