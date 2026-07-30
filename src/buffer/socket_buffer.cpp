#include "socket_buffer.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace {
    constexpr const char* ERROR_POSITION_OUT_OF_RANGE = "Position out of range!";
}

namespace impetus {
    namespace buffer {
        SocketBuffer::SocketBuffer(Memory memory, Capacity capacity) noexcept
            : size_(0)
            , capacity_(static_cast<std::size_t>(memory) * static_cast<std::size_t>(capacity)) {
            try {
                data_ = std::make_unique<char[]>(capacity_);
            } catch (const std::bad_alloc&) {
                data_ = nullptr;
                capacity_ = 0;
            }
        }

        BufferCode SocketBuffer::append(const char* src, std::size_t size) {
            if (!is_valid()) {
                return BufferCode::ERR_ALLOC;
            }

            if (src == nullptr || size == 0) {
                return BufferCode::OK;
            }

            if (size > free_space()) {
                return BufferCode::ERR_FULL;
            }

            std::memcpy(data_.get() + size_, src, size);
            size_ += size;
            return BufferCode::OK;
        }

        void SocketBuffer::erase(std::size_t position, std::size_t size) {
            assert(position <= size_ && ERROR_POSITION_OUT_OF_RANGE);

            const std::size_t remain = size_ - position;
            const std::size_t to_erase = std::min(size, remain);

            if (to_erase >= remain) {
                size_ = position;
            } else {
                std::memmove(
                    data_.get() + position, data_.get() + position + to_erase, remain - to_erase);
                size_ -= to_erase;
            }
        }
    }
}
