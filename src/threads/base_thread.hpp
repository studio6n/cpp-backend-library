#pragma once

#include "thread_configuration.hpp"

namespace impetus {
    namespace threads {
        /**
         * @brief
         * Base class for all thread types in this framework.
         *
         * Holds ThreadConfiguration and applies name, CPU affinity, and scheduling
         * policy to the OS thread. Subclasses call apply_config() from inside their
         * thread body to activate the configuration on the running OS thread.
         */
        class BaseThread {
            public:
            /**
             * @brief
             * Default construction is disabled — a ThreadConfiguration is a hard requirement.
             */
            BaseThread() = delete;

            /**
             * @brief
             * Constructs the thread with the given configuration. Does not
             * start the thread — subclasses call apply_config() from within
             * their own thread body.
             *
             * @param config
             * Configuration to apply once the thread is running.
             */
            explicit BaseThread(ThreadConfiguration config);

            /**
             * @brief
             * Copy construction is disabled.
             */
            BaseThread(const BaseThread&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            BaseThread& operator=(const BaseThread&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            BaseThread(BaseThread&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            BaseThread& operator=(BaseThread&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~BaseThread() = default;

            public:
            /**
             * @brief
             * Returns the configured thread name.
             */
            [[nodiscard]]
            const std::string& name() const noexcept;

            protected:
            /**
             * @brief
             * Applies name, CPU affinity, and scheduling policy to the calling OS thread.
             * Must be called from inside the thread body.
             *
             * @return
             * True if all steps succeeded, false if any step failed.
             */
            bool apply_config() noexcept;

            private:
            /**
             * @brief
             * Sets the OS thread name via pthread_setname_np.
             */
            bool apply_name() noexcept;

            /**
             * @brief
             * Pins the thread to config_.core_id via pthread_setaffinity_np.
             * Skipped if core_id is -1.
             */
            bool apply_affinity() noexcept;

            /**
             * @brief
             * Applies the scheduling policy and priority to the OS thread.
             * DEFAULT uses setpriority(nice_value). FIFO uses sched_setscheduler.
             */
            bool apply_scheduling() noexcept;

            protected:
            /**
             * @brief
             * Configuration applied to the OS thread at startup.
             */
            ThreadConfiguration config_;
        };
    }
}
