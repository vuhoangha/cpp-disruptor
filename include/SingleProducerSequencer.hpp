#pragma once

#include "Sequencer.hpp"
#include "Common.hpp"
#include "Util.hpp"
#include <unordered_map>
#include <cassert>

#include "SequenceGroupForSingleThread.hpp"

namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class SingleProducerSequencer final : public Sequencer {
        // quản lý các sequence đã được publish
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        // seq gần nhất đã được publisher claim
        size_t latest_claimed_sequence{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};
        const char padding1[CACHE_LINE_SIZE - sizeof(size_t)] = {};
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

        size_t next() override {
            return this->next(1);
        }


        size_t next(const size_t n) override {
            assert(sameThread() && "Accessed by two threads - use ProducerType.MULTI!");
            const size_t bufferSize = this->ringBuffer.getBufferSize();

            if (n < 1 || n > bufferSize) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t localNextValue = this->latest_claimed_sequence;
            const size_t nextSequence = localNextValue + n;
            const size_t wrapPoint = nextSequence - bufferSize;

            if (this->gating_sequences.get_cache() < wrapPoint) {
                // chờ cho tới khi consumer xử lý xong để có chỗ trống ghi dữ liệu
                while (wrapPoint > this->gating_sequences.get()) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }

            this->latest_claimed_sequence = nextSequence;

            return nextSequence;
        }


        void claim(const size_t sequence) override {
            this->latest_claimed_sequence = sequence;
        }


        void publish(const size_t sequence) override {
            this->cursor.set(sequence);
        }


        void publish(const size_t lo, const size_t hi) override {
            this->publish(hi);
        }


        [[nodiscard]] bool isAvailable(const size_t sequence) const override {
            const size_t currentSequence = this->cursor.get();
            return sequence <= currentSequence && sequence > currentSequence - this->ringBuffer.getBufferSize();
        }


        // trong các sequence barrier thì thường "nextSequence" là sequence mà barrier đang chờ, availableSequence là sequence đã được publish hoặc các dependency consmer xử lý xong
        // trong hàm waitFor của SequenceBarrier thì nextSequence <= availableSequence thì mới gọi tới đây
        [[nodiscard]] size_t getHighestPublishedSequence(const size_t nextSequence, const size_t availableSequence) const override {
            return availableSequence;
        }


        [[nodiscard]] Sequence &getCursor() {
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
