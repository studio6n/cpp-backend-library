#include "base_thread.hpp"

#include <cstddef>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>

namespace impetus {
    namespace threads {
        BaseThread::BaseThread(ThreadConfiguration config)
            : config_(std::move(config)) {
        }

        const std::string& BaseThread::name() const noexcept {
            return config_.name;
        }

        bool BaseThread::apply_config() noexcept {
            if (!apply_name()) {
                return false;
            }

            if (!apply_affinity()) {
                return false;
            }

            return apply_scheduling();
        }

        bool BaseThread::apply_name() noexcept {
            return ::pthread_setname_np(::pthread_self(), config_.name.c_str()) == 0;
        }

        bool BaseThread::apply_affinity() noexcept {
            if (config_.core_id < 0) {
                return true;
            }

            cpu_set_t cpuset{};
            CPU_ZERO(&cpuset);
            CPU_SET(static_cast<std::size_t>(config_.core_id), &cpuset);

            return ::pthread_setaffinity_np(::pthread_self(), sizeof(cpuset), &cpuset) == 0;
        }

        bool BaseThread::apply_scheduling() noexcept {
            if (config_.policy == SchedulingPolicy::DEFAULT) {
                return ::setpriority(PRIO_PROCESS, 0, config_.nice_value) == 0;
            }

            // FIFO real-time scheduling
            sched_param param{};
            param.sched_priority = config_.rt_priority;
            return ::sched_setscheduler(0, SCHED_FIFO, &param) == 0;
        }
    }
}
