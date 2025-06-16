#pragma once

#include "../sequence/SequenceGroupForMultiThread.hpp"
#include "Sequencer.hpp"
#include "../common/Util.hpp"

/**
 * cursor: vị trí cao nhất đã được producer claim nhưng chưa chắc đã được publish
 * availableBuffer: Lưu số vòng quay tương ứng vị trí trong ring_buffer để biết 1 sequence đã được publish hay chưa
 */
namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class MultiProducerSequencer final : public Sequencer {
        // quản lý các sequence đã được publisher claim. Seq ở đây tăng ko đồng nghĩa với việc seq đã được publish đâu nhé.
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        // chính là buffer_size - 1. Dùng để tính toán vị trí trong ring buffer tương tự phép chía lấy dư
        const size_t index_mask;
        // dùng để tính xem sequence hiện tại đã quay được bao nhiêu vòng bằng dịch phải indexShift bit. Ví dụ ring_buffer = 16 thì indexShift = 4 --> sequence=89 --> 89>>5=5 --> sequence này đã quay được 5 vòng
        const size_t index_shift;
        const char padding_2[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>) * 2] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        // mảng chứa số vòng quay của từng vị trí trong RingBuffer. Nó sẽ báo hiệu sequence này được publish hay chưa
        // vì cần tính chất atomic + memory barriers nên Sequence hoàn toàn đáp ứng
        std::array<Sequence, RING_BUFFER_SIZE> available_buffer;
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer;
        SequenceGroupForMultiThread<NUMBER_GATING_SEQUENCES> gating_sequences;

    public:
        explicit MultiProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer_ptr)
            : index_mask(ring_buffer_ptr.get_buffer_size() - 1), index_shift(Util::log_2(ring_buffer_ptr.get_buffer_size())), ring_buffer(ring_buffer_ptr) {
            for (auto &seq: available_buffer) {
                seq.set(Util::calculate_initial_value_sequence(RING_BUFFER_SIZE));
            }
        }

        void add_gating_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) override {
            this->gating_sequences.set_sequences(sequences);
        }

        void claim(const size_t sequence) override {
            this->cursor.set(sequence);
        }

        size_t next() override {
            return next(1);
        }

        size_t next(const size_t n) override {
            const size_t buffer_size = this->ring_buffer.get_buffer_size();

            if (n < 1 || n > buffer_size) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t current_sequence = this->cursor.get_and_add_relax(n);
            const size_t next_sequence = current_sequence + n;
            const size_t wrap_point = next_sequence - buffer_size;
            const size_t cached_gating_sequence = this->gating_sequences.get_cache();

            if (cached_gating_sequence < wrap_point) {
                while (this->gating_sequences.get() < wrap_point) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }

            return next_sequence;
        }

        void publish(const size_t sequence) override {
            this->set_available(sequence);
        }

        void publish(const size_t low, const size_t high) override {
            for (size_t i = low; i <= high; ++i) {
                this->set_available(i);
            }
        }

        void set_available(const size_t sequence) {
            const size_t index = calculate_index(sequence);
            const size_t flag = calculate_availability_flag(sequence);
            available_buffer[index].set(flag);
        }

        [[nodiscard]] size_t calculate_availability_flag(const size_t sequence) const {
            return sequence >> index_shift;
        }

        [[nodiscard]] size_t calculate_index(const size_t sequence) const {
            return sequence & index_mask;
        }

        [[nodiscard]] bool is_available(const size_t sequence) const override {
            const size_t index = calculate_index(sequence);
            const size_t flag = calculate_availability_flag(sequence);
            return available_buffer[index].get() == flag;
        }

        [[nodiscard]] Sequence &get_cursor() {
            return cursor;
        }

        /**
         * lấy sequence cao nhất đã được publish để consumer xử lý
         * vì trong môi trường multi producer thì có thể sequence 10 đã được producer A publish còn sequence 9 do producer B đảm nhiệm vẫn đang xử lý
         */
        [[nodiscard]] size_t get_highest_published_sequence(const size_t lower_bound, const size_t available_sequence) const override {
            for (size_t sequence = lower_bound; sequence <= available_sequence; ++sequence) {
                if (!is_available(sequence)) {
                    return sequence - 1;
                }
            }
            return available_sequence;
        }
    };
}
