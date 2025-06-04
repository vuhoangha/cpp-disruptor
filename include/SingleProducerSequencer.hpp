#pragma once

#include "AbstractSequencer.hpp"
#include "Common.hpp"
#include "Util.hpp"
#include <unordered_map>
#include "InsufficientCapacityException.hpp"
#include <cassert>

namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class SingleProducerSequencer : public AbstractSequencer<T, RING_BUFFER_SIZE, NUMBER_GATING_SEQUENCES> {
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE];
        int64_t nextValue{Sequencer::INITIAL_CURSOR_VALUE}; // seq gần nhất đã được publisher claim
        int64_t cachedValue{Sequencer::INITIAL_CURSOR_VALUE}; // seq nhỏ nhất đã được các gatingSequences xử lý
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) * 2];
        const char padding3[CACHE_LINE_SIZE];


        bool sameThread() {
            return ProducerThreadAssertion::isSameThreadProducingTo(this);
        }

    public:
        explicit SingleProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer) : AbstractSequencer<T, RING_BUFFER_SIZE, NUMBER_GATING_SEQUENCES>(ringBuffer) {
        }


        int64_t next() override {
            return this->next(1);
        }


        int64_t next(const int n) override {
            assert(sameThread() && "Accessed by two threads - use ProducerType.MULTI!");
            const int32_t bufferSize = this->ringBuffer.getBufferSize();

            if (n < 1 || n > bufferSize) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const int64_t localNextValue = this->nextValue;
            const int64_t nextSequence = localNextValue + n;
            const int64_t wrapPoint = nextSequence - bufferSize;
            const int64_t cachedGatingSequence = this->cachedValue;

            if (cachedGatingSequence < wrapPoint) {
                // chờ cho tới khi consumer xử lý xong để có chỗ trống ghi dữ liệu
                int64_t minSequence;
                while (wrapPoint > (minSequence = Util::getMinimumSequence(gatingSequences, localNextValue))) {
                    // TODO: Use waitStrategy to spin?
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
                this->cachedValue = minSequence;
            }

            this->nextValue = nextSequence;

            return true;
        }


        int64_t remainingCapacity() const override {
            const long consumed = Util::getMinimumSequence(this->gatingSequences, nextValue);
            const long produced = this->nextValue;
            return this->ringBuffer.getBufferSize() - (produced - consumed);
        }


        void claim(const int64_t sequence) override {
            this->nextValue = sequence;
        }


        void publish(const int64_t sequence) override {
            this->cursor.set(sequence);
        }


        void publish(const int64_t lo, const int64_t hi) override {
            this->publish(hi);
        }


        bool isAvailable(const int64_t sequence) override {
            const int64_t currentSequence = this->cursor.get();
            return sequence <= currentSequence && sequence > currentSequence - this->ringBuffer.getBufferSize();
        }


        // trong các sequence barrier thì thường "nextSequence" là sequence mà barrier đang chờ, availableSequence là sequence đã được publish hoặc các dependency consmer xử lý xong
        // trong hàm waitFor của SequenceBarrier thì nextSequence <= availableSequence thì mới gọi tới đây
        int64_t getHighestPublishedSequence(const int64_t nextSequence, const int64_t availableSequence) const override {
            return availableSequence;
        }


        /**
        * Only used when assertions are enabled.
        */
        class ProducerThreadAssertion {
        private:
            /**
             * Tracks the threads publishing to SingleProducerSequencers to identify if more than one
             * thread accesses any SingleProducerSequencer.
             * I.e. it helps developers detect early if they use the wrong ProducerType.
             */
            static inline std::unordered_map<SingleProducerSequencer *, std::thread::id> PRODUCERS;
            static inline std::mutex producersMutex;

        public:
            static bool isSameThreadProducingTo(SingleProducerSequencer *singleProducerSequencer) {
                std::lock_guard<std::mutex> lock(producersMutex);

                const std::thread::id currentThread = std::this_thread::get_id();
                if (!PRODUCERS.contains(singleProducerSequencer)) {
                    PRODUCERS[singleProducerSequencer] = currentThread;
                }

                return PRODUCERS[singleProducerSequencer] == currentThread;
            }
        };
    };
}
