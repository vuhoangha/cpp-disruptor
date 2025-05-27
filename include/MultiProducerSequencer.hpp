#pragma once

#include "AbstractSequencer.hpp"

/**
 * cursor: vị trí cao nhất đã được producer claim nhưng chưa chắc đã được publish
 * availableBuffer: Lưu số vòng quay tương ứng vị trí trong ring_buffer để biết 1 sequence đã được publish hay chưa
 */
namespace disruptor {
    template<size_t N>
    class MultiProducerSequencer : public AbstractSequencer {
        // seq nhỏ nhất đã được các gatingSequences xử lý
        alignas(CACHE_LINE_SIZE) Sequence cachedValue{Sequence::INITIAL_VALUE};

        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE];
        // chính là buffer_size - 1. Dùng để tính toán vị trí trong ring buffer tương tự phép chía lấy dư
        const int32_t indexMask;
        // dùng để tính xem sequence hiện tại đã quay được bao nhiêu vòng bằng dịch phải indexShift bit. Ví dụ ring_buffer = 16 thì indexShift = 4 --> sequence=89 --> 89>>5=5 --> sequence này đã quay được 5 vòng
        const int32_t indexShift;
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int32_t>) * 2];
        const char padding3[CACHE_LINE_SIZE];

        // mảng chứa số vòng quay của từng vị trí trong RingBuffer. Nó sẽ báo hiệu sequence này được publish hay chưa
        // vì cần tính chất atomic + memory barriers nên Sequence hoàn toàn đáp ứng
        std::array<Sequence, N> availableBuffer;
        char padding4[CACHE_LINE_SIZE * 2];

    public:
        explicit MultiProducerSequencer(const int bufferSize, const std::shared_ptr<WaitStrategy> &waitStrategy)
            : AbstractSequencer(bufferSize, waitStrategy), indexMask(bufferSize - 1), indexShift(Util::log2(bufferSize)), padding{} {
            for (auto &seq: availableBuffer) {
                seq.set(-1);
            }
        }


        void claim(const int64_t sequence) override {
            this->cursor.set(sequence);
        }


        int64_t next() override {
            return next(1);
        }


        int64_t next(const int32_t n) override {
            if (n < 1 || n > this->bufferSize) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const int64_t currentSequence = this->cursor.getAndAddRelax(n);
            const int64_t nextSequence = currentSequence + n;
            const int64_t wrapPoint = nextSequence - this->bufferSize;
            const int64_t cachedGatingSequence = this->cachedValue.getRelax();

            if (cachedGatingSequence < wrapPoint) {
                int64_t gatingSequence;
                while ((gatingSequence = Util::getMinimumSequence(this->gatingSequences, cachedGatingSequence)) < wrapPoint) {
                    // TODO: Use waitStrategy to spin?
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
                this->cachedValue.setRelax(gatingSequence);
            }

            return nextSequence;
        }


        int64_t remainingCapacity() const override {
            const int64_t currentSequence = this->cursor.getRelax();
            const int64_t consumed = Util::getMinimumSequence(this->gatingSequences, currentSequence);
            const int64_t produced = currentSequence;
            return getBufferSize() - (produced - consumed);
        }


        void publish(const int64_t sequence) override {
            this->setAvailable(sequence);
            this->waitStrategy->signalAllWhenBlocking();
        }


        void publish(const int64_t low, const int64_t high) override {
            for (int64_t i = low; i <= high; ++i) {
                this->setAvailable(i);
            }
            this->waitStrategy->signalAllWhenBlocking();
        }


        void setAvailable(const int64_t sequence) {
            const int32_t index = calculateIndex(sequence);
            const int64_t flag = calculateAvailabilityFlag(sequence);
            availableBuffer[index].store(flag, std::memory_order_release);
        }


        int64_t calculateAvailabilityFlag(const int64_t sequence) const {
            return sequence >> indexShift;
        }


        int32_t calculateIndex(const int64_t sequence) const {
            return static_cast<int>(sequence) & indexMask;
        }


        bool isAvailable(const int64_t sequence) const override {
            const int32_t index = calculateIndex(sequence);
            const int64_t flag = calculateAvailabilityFlag(sequence);
            return availableBuffer[index].load(std::memory_order_acquire) == flag;
        }


        /**
         * lấy sequence cao nhất đã được publish để consumer xử lý
         * vì trong môi trường multi producer thì có thể sequence 10 đã được producer A publish còn sequence 9 do producer B đảm nhiệm vẫn đang xử lý
         */
        int64_t getHighestPublishedSequence(const int64_t lowerBound, const int64_t availableSequence) const override {
            for (int64_t sequence = lowerBound; sequence <= availableSequence; ++sequence) {
                if (!isAvailable(sequence)) {
                    return sequence - 1;
                }
            }
            return availableSequence;
        }
    };
}
