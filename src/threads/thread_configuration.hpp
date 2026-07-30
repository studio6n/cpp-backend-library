#pragma once

#include <cstdint>
#include <string>

namespace impetus {
    namespace threads {
        /**
         * @brief
         * Scheduling policy applied to a thread at creation.
         */
        enum class SchedulingPolicy : std::uint8_t {
            DEFAULT, // SCHED_OTHER — controlled by nice_value
            FIFO // SCHED_FIFO  — real-time, requires CAP_SYS_NICE
        };

        /**
         * @brief
         * Configuration applied to an OS thread on startup.
         *
         * name must not be empty and must not exceed 15 characters
         * (pthread_setname_np limit). nice_value is only used when policy
         * is DEFAULT. rt_priority is only used when policy is FIFO.
         */
        struct ThreadConfiguration {
            /**
             * @brief
             * Thread name. Must not be empty and must not exceed 15 characters.
             */
            std::string name;

            /**
             * @brief
             * CPU core to pin the thread to. -1 disables pinning.
             */
            int core_id = -1;

            /**
             * @brief
             * Scheduling policy to apply.
             */
            SchedulingPolicy policy = SchedulingPolicy::DEFAULT;

            /**
             * @brief
             * Nice value applied when policy is DEFAULT.
             */
            int nice_value = 0;

            /**
             * @brief
             * Real-time priority applied when policy is FIFO.
             */
            int rt_priority = 1;
        };
    }
}
