#pragma once

#include <functional>
#include "Common.hpp"
#include "Sequencer.hpp"

// chứa bộ đệm vòng với padding kỹ lưỡng
namespace disruptor {
    template<typename T, size_t N>
    class RingBuffer {
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        int64_t indexMask;
        int32_t bufferSize;
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) - sizeof(std::atomic<int32_t>)] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

        std::array<T, N> entries;
        char padding4[CACHE_LINE_SIZE * 2];

        std::function<T()> eventFactory;

    public:
        explicit RingBuffer(const int32_t bufferSize) : bufferSize(bufferSize), eventFactory(std::move(eventFactory)) {
            this->indexMask = this->bufferSize - 1;

            if (this->bufferSize < 1) {
                throw std::invalid_argument("bufferSize must be > 0");
            }
            if (std::popcount(static_cast<unsigned int>(this->bufferSize)) != 1) {
                throw std::invalid_argument("bufferSize must be a power of 2");
            }

            // fill data to array
            for (int i = 0; i < this->bufferSize; i++) {
                this->entries[i] = this->eventFactory();
            }
        }

        T get(const int64_t sequence) const {
            return this->entries[sequence & this->indexMask];
        }

        int32_t getBufferSize() const {
            return this->bufferSize;
        }
    };
}
