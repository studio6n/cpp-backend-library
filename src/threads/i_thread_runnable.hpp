#pragma once

namespace impetus {
    namespace threads {
        /**
         * @brief
         * Interface for classes that define long-running work on a dedicated thread.
         *
         * Implement run() with the work to be executed. Pass to WorkerThread via
         * lambda: thread.start([&runnable]{ runnable.run(); }).
         */
        class IThreadRunnable {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            IThreadRunnable(const IThreadRunnable&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            IThreadRunnable& operator=(const IThreadRunnable&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            IThreadRunnable(IThreadRunnable&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            IThreadRunnable& operator=(IThreadRunnable&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~IThreadRunnable() = default;

            public:
            /**
             * @brief
             * Called repeatedly by the WorkerThread loop until the thread is stopped.
             */
            virtual void run() = 0;

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            IThreadRunnable() = default;
        };
    }
}
