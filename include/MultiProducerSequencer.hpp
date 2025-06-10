#pragma once

#include "SequenceGroupForMultiThread.hpp"
#include "Sequencer.hpp"
#include "Util.hpp"

/**
 * cursor: vị trí cao nhất đã được producer claim nhưng chưa chắc đã được publish
 * availableBuffer: Lưu số vòng quay tương ứng vị trí trong ring_buffer để biết 1 sequence đã được publish hay chưa
 */
namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class MultiProducerSequencer final : public Sequencer {
        // quản lý các sequence đã được publisher claim. Seq ở đây tăng ko đồng nghĩa với việc seq đã được publish đâu nhé.
        alignas(CACHE_LINE_SIZE) Sequence cursor{INITIAL_CURSOR_VALUE};

        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        // chính là buffer_size - 1. Dùng để tính toán vị trí trong ring buffer tương tự phép chía lấy dư
        const int64_t indexMask;
        // dùng để tính xem sequence hiện tại đã quay được bao nhiêu vòng bằng dịch phải indexShift bit. Ví dụ ring_buffer = 16 thì indexShift = 4 --> sequence=89 --> 89>>5=5 --> sequence này đã quay được 5 vòng
        const int64_t indexShift;
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) * 2] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

        // mảng chứa số vòng quay của từng vị trí trong RingBuffer. Nó sẽ báo hiệu sequence này được publish hay chưa
        // vì cần tính chất atomic + memory barriers nên Sequence hoàn toàn đáp ứng
        std::array<Sequence, RING_BUFFER_SIZE> availableBuffer;
        const char padding4[CACHE_LINE_SIZE * 2] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer;
        SequenceGroupForMultiThread<NUMBER_GATING_SEQUENCES> gating_sequences;

    public:
        explicit MultiProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer)
            : indexMask(ringBuffer.getBufferSize() - 1), indexShift(Util::log2(ringBuffer.getBufferSize())), ringBuffer(ringBuffer) {
            for (auto &seq: availableBuffer) {
                seq.set(-1);
            }
        }

        void addGatingSequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) override {
            this->gating_sequences.setSequences(sequences);
        }

        void claim(const int64_t sequence) override {
            this->cursor.set(sequence);
        }

        int64_t next() override {
            return next(1);
        }


        int64_t next(const int64_t n) override {
            const int64_t bufferSize = this->ringBuffer.getBufferSize();

            if (n < 1 || n > bufferSize) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const int64_t currentSequence = this->cursor.getAndAddRelax(n);
            const int64_t nextSequence = currentSequence + n;
            const int64_t wrapPoint = nextSequence - bufferSize;
            const int64_t cachedGatingSequence = this->gating_sequences.get_cache();

            if (cachedGatingSequence < wrapPoint) {
                while (this->gating_sequences.get() < wrapPoint) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }

            return nextSequence;
        }


        void publish(const int64_t sequence) override {
            this->setAvailable(sequence);
        }


        void publish(const int64_t low, const int64_t high) override {
            for (int64_t i = low; i <= high; ++i) {
                this->setAvailable(i);
            }
        }


        void setAvailable(const int64_t sequence) {
            const int64_t index = calculateIndex(sequence);
            const int64_t flag = calculateAvailabilityFlag(sequence);
            availableBuffer[index].set(flag);
        }


        [[nodiscard]] int64_t calculateAvailabilityFlag(const int64_t sequence) const {
            return sequence >> indexShift;
        }


        [[nodiscard]] int64_t calculateIndex(const int64_t sequence) const {
            return sequence & indexMask;
        }


        [[nodiscard]] bool isAvailable(const int64_t sequence) const override {
            const int64_t index = calculateIndex(sequence);
            const int64_t flag = calculateAvailabilityFlag(sequence);
            return availableBuffer[index].get() == flag;
        }


        [[nodiscard]] Sequence &getCursor() {
            return cursor;
        }


        /**
         * lấy sequence cao nhất đã được publish để consumer xử lý
         * vì trong môi trường multi producer thì có thể sequence 10 đã được producer A publish còn sequence 9 do producer B đảm nhiệm vẫn đang xử lý
         */
        [[nodiscard]] int64_t getHighestPublishedSequence(const int64_t lowerBound, const int64_t availableSequence) const override {
            for (int64_t sequence = lowerBound; sequence <= availableSequence; ++sequence) {
                if (!isAvailable(sequence)) {
                    return sequence - 1;
                }
            }
            return availableSequence;
        }
    };
}
