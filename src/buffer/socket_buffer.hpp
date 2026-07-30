#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

#include "buffer_utility.hpp"

namespace impetus {
    namespace buffer {
        /**
         * @brief
         * Fixed-size byte buffer for socket I/O staging.
         *
         * The full allocation (Memory * Capacity blocks) is made once at construction
         * and never changes. This gives a hard upper bound on memory usage per socket,
         * making the server's total footprint predictable and auditable.
         *
         * When the buffer is full, operations that would require more space return
         * BufferCode::ERR_FULL. The composing class is responsible for deciding what
         * to do — drop the connection, apply backpressure, etc.
         *
         * Not copyable or movable. Intended to live as a member of a socket class.
         */
        class SocketBuffer final {
            public:
            /**
             * @brief
             * Base block size in bytes.
             */
            enum class Memory : std::size_t {
                XSMALL = 256,
                SMALL = XSMALL * 2,
                MEDIUM = SMALL * 2,
                LARGE = MEDIUM * 2,
                XLARGE = LARGE * 2
            };

            /**
             * @brief
             * Maximum number of blocks the buffer may hold.
             */
            enum class Capacity : std::size_t {
                XSMALL = 1,
                SMALL = XSMALL * 2,
                MEDIUM = SMALL * 2,
                LARGE = MEDIUM * 2,
                XLARGE = LARGE * 2
            };

            public:
            /**
             * @brief
             * Constructs a buffer and allocates Memory * Capacity bytes.
             *
             * If allocation fails, all operations will return BufferCode::ERR_ALLOC.
             *
             * @param memory
             * Block size in bytes.
             *
             * @param capacity
             * Number of blocks to allocate.
             */
            explicit SocketBuffer(Memory memory = Memory::MEDIUM,
                                  Capacity capacity = Capacity::MEDIUM) noexcept;

            /**
             * @brief
             * Copy construction is disabled.
             */
            SocketBuffer(const SocketBuffer&) = delete;

            /**
             * @brief
             * Copy assignment is disabled.
             */
            SocketBuffer& operator=(const SocketBuffer&) = delete;

            /**
             * @brief
             * Move construction is disabled.
             */
            SocketBuffer(SocketBuffer&&) = delete;

            /**
             * @brief
             * Move assignment is disabled.
             */
            SocketBuffer& operator=(SocketBuffer&&) = delete;

            /**
             * @brief
             * Default destructor.
             */
            ~SocketBuffer() = default;

            public:
            /**
             * @brief
             * Returns the number of bytes currently held.
             */
            [[nodiscard]]
            std::size_t size() const noexcept {
                return size_;
            }

            /**
             * @brief
             * Returns the total fixed capacity in bytes. Never changes after construction.
             */
            [[nodiscard]]
            std::size_t capacity() const noexcept {
                return capacity_;
            }

            /**
             * @brief
             * Returns the number of bytes available for writing.
             */
            [[nodiscard]]
            std::size_t free_space() const noexcept {
                return capacity_ - size_;
            }

            /**
             * @brief
             * Returns true if the buffer holds no bytes.
             */
            [[nodiscard]]
            bool empty() const noexcept {
                return size_ == 0;
            }

            /**
             * @brief
             * Returns true if the buffer has reached its capacity.
             */
            [[nodiscard]]
            bool full() const noexcept {
                return size_ == capacity_;
            }

            /**
             * @brief
             * Returns true if the internal allocation succeeded at construction.
             */
            [[nodiscard]]
            bool is_valid() const noexcept {
                return data_.get() != nullptr;
            }

            /**
             * @brief
             * Returns a read-only pointer to the start of buffered data.
             */
            [[nodiscard]]
            const char* data() const noexcept {
                return data_.get();
            }

            /**
             * @brief
             * Returns a writable pointer to the next available byte.
             * Advance with commit() after writing.
             */
            [[nodiscard]]
            char* write_ptr() noexcept {
                return data_.get() + size_;
            }

            /**
             * @brief
             * Resets the buffer to empty without deallocating.
             */
            void clear() noexcept {
                size_ = 0;
            }

            /**
             * @brief
             * Marks n bytes written via write_ptr() as committed into the buffer.
             *
             * @param n
             * Number of bytes to commit. Must not exceed free_space().
             */
            void commit(std::size_t n) noexcept {
                assert(n <= free_space());
                size_ += n;
            }

            public:
            /**
             * @brief
             * Appends bytes to the end of the buffer.
             *
             * Returns BufferCode::ERR_FULL if there is insufficient free space.
             *
             * @param src
             * Pointer to the source data.
             *
             * @param size
             * Number of bytes to append.
             *
             * @return
             * BufferCode::OK on success, error code otherwise.
             */
            [[nodiscard]]
            BufferCode append(const char* src, std::size_t size);

            /**
             * @brief
             * Removes bytes from the buffer starting at the given position.
             *
             * @param position
             * Start of the region to erase.
             *
             * @param size
             * Number of bytes to erase. Defaults to K_BUFFER_NPOS (erase to end).
             */
            void erase(std::size_t position, std::size_t size = K_BUFFER_NPOS);

            private:
            /**
             * @brief
             * Fixed-size heap allocation.
             */
            std::unique_ptr<char[]> data_;

            private:
            /**
             * @brief
             * Number of bytes currently written into the buffer.
             */
            std::size_t size_;

            /**
             * @brief
             * Total fixed capacity in bytes. Set at construction and never changes.
             */
            std::size_t capacity_;
        };
    }
}
