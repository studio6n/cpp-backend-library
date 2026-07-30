#pragma once

namespace impetus {
    namespace dispatch {
        /**
         * @brief
         * Interface for a consumer of items produced by an IProducer<T>.
         *
         * Register with a producer via IProducer<T>::add_consumer(). consume()
         * is invoked once per dispatched item, on the calling thread of dispatch().
         */
        template <typename T>
        class IConsumer {
            public:
            /**
             * @brief
             * Copy construction is disabled.
             */
            IConsumer(const IConsumer&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            IConsumer& operator=(const IConsumer&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            IConsumer(IConsumer&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            IConsumer& operator=(IConsumer&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            virtual ~IConsumer() = default;

            public:
            /**
             * @brief
             * Called by the producer when a new item is available.
             *
             * @param item
             * The produced item to consume.
             */
            virtual void consume(const T& item) noexcept = 0;

            protected:
            /**
             * @brief
             * Default constructor. Only accessible to subclasses.
             */
            IConsumer() = default;
        };
    }
}
