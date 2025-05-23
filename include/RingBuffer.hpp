#pragma once

#include <functional>

#include "Common.hpp"
#include "Sequencer.hpp"

namespace disruptor {
    template<typename T, size_t N>
    class RingBuffer {
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE];
        int64_t indexMask;
        int32_t bufferSize;
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) - sizeof(std::atomic<int32_t>)];
        const char padding3[CACHE_LINE_SIZE];

        std::array<T, N> entries;
        char padding4[CACHE_LINE_SIZE * 2];

        alignas(CACHE_LINE_SIZE) const char padding5[CACHE_LINE_SIZE];
        Sequencer &sequencer;
        char padding6[CACHE_LINE_SIZE * 2];

        std::function<T()> eventFactory;

    public:
        explicit RingBuffer(Sequencer &sequencer, std::function<T()> &&eventFactory)
            : sequencer(sequencer), eventFactory(std::move(eventFactory)) {
            this->bufferSize = this->sequencer.getBufferSize();
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

        int64_t next() const {
            return this->sequencer.next();
        }

        int64_t next(const int32_t n) const {
            return this->sequencer.next(n);
        }

        bool isAvailable(const int64_t sequence) const {
            return this->sequencer.isAvailable(sequence);
        }

        void addGatingSequences(const std::vector<std::shared_ptr<Sequence> > &gatingSequences) const {
            this->sequencer.addGatingSequences(gatingSequences);
        }
    };
}
