#pragma once

#include "AbstractSequencer.hpp"

/**
 * cursor: vị trí cao nhất đã được producer claim nhưng chưa chắc đã được publish
 * availableBuffer: Lưu số vòng quay tương ứng vị trí trong ring_buffer để biết 1 sequence đã được publish hay chưa
 */
namespace disruptor {
    template<size_t N>
    class MultiProducerSequencer : public AbstractSequencer {
    private:
        // seq nhỏ nhất đã được các gatingSequences xử lý
        alignas(CACHE_LINE_SIZE) Sequence cachedValue{Sequence::INITIAL_VALUE};

        // chính là buffer_size - 1. Dùng để tính toán vị trí trong ring buffer tương tự phép chía lấy dư
        alignas(CACHE_LINE_SIZE) const int32_t indexMask;
        // dùng để tính xem sequence hiện tại đã quay được bao nhiêu vòng bằng dịch phải indexShift bit. Ví dụ ring_buffer = 16 thì indexShift = 4 --> sequence=89 --> 89>>5=5 --> sequence này đã quay được 5 vòng
        const int32_t indexShift;
        char padding[CACHE_LINE_SIZE - sizeof(std::atomic<int32_t>) * 2];

        // mảng chứa số vòng quay của từng vị trí trong RingBuffer. Nó sẽ báo hiệu sequence này được publish hay chưa
        // vì cần tính chất atomic + memory barriers nên Sequence hoàn toàn đáp ứng
        alignas(CACHE_LINE_SIZE) std::array<Sequence, N> availableBuffer;

    public:
        MultiProducerSequencer(const int bufferSize, std::shared_ptr<WaitStrategy> waitStrategy)
            : AbstractSequencer(bufferSize, waitStrategy), indexMask(bufferSize - 1), indexShift(Util::log2(bufferSize)) {
            for (auto &seq: availableBuffer) {
                seq.set(-1);
            }
        }


        bool hasAvailableCapacity(const int requiredCapacity) const override {
            const int64_t lastClaimedSequence = this->cursor.getRelax();
            const int64_t wrapPoint = lastClaimedSequence + requiredCapacity - this->bufferSize;

            if (const int64_t cachedGatingSequence = this->cachedValue.getRelax(); cachedGatingSequence < wrapPoint) {
                const int64_t minSequence = Util::getMinimumSequence(this->gatingSequences, cachedGatingSequence);
                this->cachedValue.setRelax(minSequence);

                if (minSequence < wrapPoint) {
                    return false;
                }
            }

            return true;
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

            if (const int64_t cachedGatingSequence = this->cachedValue.getRelax(); cachedGatingSequence < wrapPoint) {
                int64_t gatingSequence;
                while ((gatingSequence = Util::getMinimumSequence(this->gatingSequences, cachedGatingSequence)) < wrapPoint) {
                    // TODO: Use waitStrategy to spin?
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
                this->cachedValue.setRelax(gatingSequence);
            }

            return nextSequence;
        }

        tiếp tục các hàm try_next

    };
}
