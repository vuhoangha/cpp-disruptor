#pragma once

#include <unordered_map>
#include "SequenceBarrier.hpp"
#include "Sequence.hpp"
#include "Sequencer.hpp"
#include "FixedSequenceGroup.hpp"
#include "AlertException.hpp"
#include "WaitStrategyType.hpp"
#include "BusySpinWaitStrategy.hpp"
#include "SequenceGroupForSingleThread.hpp"
#include "SleepingWaitStrategy.hpp"
#include "YieldingWaitStrategy.hpp"

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
        using Strategy = typename WaitStrategySelector<T, NUMBER_DEPENDENT_SEQUENCES>::type;
        Strategy waitStrategy;
        bool alerted;
        Sequencer &sequencer;
        SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> dependent_sequences;

        bool sameThread() {
            return SequenceBarrierThreadAssertion::isSameThread(this);
        }

    public:
        ProcessingSequenceBarrier(
            Sequencer &sequencer,
            const WaitStrategyType waitStrategyType,
            const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences)
            : alerted(false),
              sequencer(sequencer),
              dependent_sequences(dependent_sequences) {
        }


        int64_t waitFor(int64_t sequence) override {
            assert(sameThread() && "Accessed by two threads");
            checkAlert();

            const int64_t availableSequence = waitStrategy.waitFor(sequence, this->dependent_sequences, *this);
            if (availableSequence < sequence) {
                return availableSequence;
            }

            return sequencer.getHighestPublishedSequence(sequence, availableSequence);
        }


        int64_t getCursor() const override {
            return this->dependent_sequences.get();
        }


        bool isAlerted() const override {
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
