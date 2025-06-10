#pragma once

#include "Sequencer.hpp"
#include "Common.hpp"
#include "Util.hpp"
#include <unordered_map>
#include <cassert>

#include "SequenceGroupForSingleThread.hpp"

namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class SingleProducerSequencer : public Sequencer {
        // quản lý các sequence đã được publish
        alignas(CACHE_LINE_SIZE) Sequence cursor{INITIAL_VALUE_SEQUENCE};

        // seq gần nhất đã được publisher claim
        int64_t latest_claimed_sequence{INITIAL_VALUE_SEQUENCE};
        const char padding1[CACHE_LINE_SIZE - sizeof(int64_t)] = {};
        const char padding2[CACHE_LINE_SIZE] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer;
        SequenceGroupForSingleThread<NUMBER_GATING_SEQUENCES> gating_sequences;

        bool sameThread() {
            return ProducerThreadAssertion::isSameThreadProducingTo(this);
        }

    public:
        explicit SingleProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer) : ringBuffer(ringBuffer) {
        }

        void addGatingSequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) override {
            this->gating_sequences.setSequences(sequences);
        }

        int64_t next() override {
            return this->next(1);
        }


        int64_t next(const int64_t n) override {
            assert(sameThread() && "Accessed by two threads - use ProducerType.MULTI!");
            const int32_t bufferSize = this->ringBuffer.getBufferSize();

            if (n < 1 || n > bufferSize) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const int64_t localNextValue = this->latest_claimed_sequence;
            const int64_t nextSequence = localNextValue + n;
            const int64_t wrapPoint = nextSequence - bufferSize;

            if (this->gating_sequences.get_cache() < wrapPoint) {
                // chờ cho tới khi consumer xử lý xong để có chỗ trống ghi dữ liệu
                while (wrapPoint > this->gating_sequences.get()) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }

            this->latest_claimed_sequence = nextSequence;

            return nextSequence;
        }


        void claim(const int64_t sequence) override {
            this->latest_claimed_sequence = sequence;
        }


        void publish(const int64_t sequence) override {
            this->cursor.set(sequence);
        }


        void publish(const int64_t lo, const int64_t hi) override {
            this->publish(hi);
        }


        bool isAvailable(const int64_t sequence) const override {
            const int64_t currentSequence = this->cursor.get();
            return sequence <= currentSequence && sequence > currentSequence - this->ringBuffer.getBufferSize();
        }


        // trong các sequence barrier thì thường "nextSequence" là sequence mà barrier đang chờ, availableSequence là sequence đã được publish hoặc các dependency consmer xử lý xong
        // trong hàm waitFor của SequenceBarrier thì nextSequence <= availableSequence thì mới gọi tới đây
        [[nodiscard]] int64_t getHighestPublishedSequence(const int64_t nextSequence, const int64_t availableSequence) const override {
            return availableSequence;
        }


        [[nodiscard]] Sequence& getCursor() {
            return cursor;
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
