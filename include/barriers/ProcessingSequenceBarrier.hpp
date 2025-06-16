#pragma once

#include <unordered_map>
#include "SequenceBarrier.hpp"
#include "../sequence/Sequence.hpp"
#include "../sequencer/Sequencer.hpp"
#include "../exception/AlertException.hpp"
#include "../wait_strategy/WaitStrategyType.hpp"
#include "../wait_strategy/BusySpinWaitStrategy.hpp"
#include "../sequence/SequenceGroupForSingleThread.hpp"
#include "../wait_strategy/SleepingWaitStrategy.hpp"
#include "../wait_strategy/YieldingWaitStrategy.hpp"

/**
 * mỗi processor sẽ có 1 sequence barrier duy nhất, nhằm tối ưu hóa cho các giá trị cache phía trong
 */
namespace disruptor {
    template<WaitStrategyType T, size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector;

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> {
        using type = BusySpinWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::SLEEP, NUMBER_DEPENDENT_SEQUENCES> {
        using type = SleepingWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::YIELD, NUMBER_DEPENDENT_SEQUENCES> {
        using type = YieldingWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<WaitStrategyType T, size_t NUMBER_DEPENDENT_SEQUENCES>
    class ProcessingSequenceBarrier final : public SequenceBarrier {
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        using Strategy = typename WaitStrategySelector<T, NUMBER_DEPENDENT_SEQUENCES>::type;
        Strategy waitStrategy;
        // lắng nghe trực tiếp sự kiện từ publisher chứ ko phải từ 1 dependent processor nào khác
        const bool direct_publisher_event_listener;
        const char padding2[CACHE_LINE_SIZE * 2] = {};

        alignas(CACHE_LINE_SIZE) SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> dependent_sequences;

        alignas(CACHE_LINE_SIZE) const char padding3[CACHE_LINE_SIZE] = {};
        bool alerted;
        const char padding4[CACHE_LINE_SIZE - sizeof(bool)] = {};
        const char padding5[CACHE_LINE_SIZE] = {};

        Sequencer &sequencer;


        bool sameThread() {
            return SequenceBarrierThreadAssertion::isSameThread(this);
        }

    public:
        ProcessingSequenceBarrier(
            const bool direct_publisher_event_listener,
            std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences,
            Sequencer &sequencer)
            : direct_publisher_event_listener(direct_publisher_event_listener),
              dependent_sequences(dependent_sequences),
              alerted(false),
              sequencer(sequencer) {
        }


        size_t waitFor(size_t sequence) override {
            assert(sameThread() && "Accessed by two threads");
            checkAlert();

            const size_t availableSequence = waitStrategy.waitFor(sequence, this->dependent_sequences, *this);
            if (availableSequence < sequence) {
                return availableSequence;
            }

            if (direct_publisher_event_listener) {
                return sequencer.getHighestPublishedSequence(sequence, availableSequence);
            } else {
                // nếu đã dependent vào processor khác thì khi processor kia xử lý xong thì ta xử lý luôn, vì chắc chắn event đã được publish rồi
                return availableSequence;
            }
        }


        [[nodiscard]] size_t getCursor() override {
            return this->dependent_sequences.get();
        }


        [[nodiscard]] bool isAlerted() const override {
            return alerted;
        }


        void alert() override {
            alerted = true;
        }


        void clearAlert() override {
            alerted = false;
        }


        void checkAlert() const override {
            if (alerted) {
                throw AlertException();
            }
        }


        /**
       * Only used when assertions are enabled.
       */
        class SequenceBarrierThreadAssertion {
        private:
            static inline std::unordered_map<ProcessingSequenceBarrier *, std::thread::id> SEQUENCE_BARRIERS;
            static inline std::mutex producersMutex;

        public:
            static bool isSameThread(ProcessingSequenceBarrier *sequenceBarrier) {
                std::lock_guard<std::mutex> lock(producersMutex);

                const std::thread::id currentThread = std::this_thread::get_id();
                if (!SEQUENCE_BARRIERS.contains(sequenceBarrier)) {
                    SEQUENCE_BARRIERS[sequenceBarrier] = currentThread;
                }

                return SEQUENCE_BARRIERS[sequenceBarrier] == currentThread;
            }
        };
    };
}
