#pragma once

#include <atomic>
#include <vector>
#include "SequenceBarrier.hpp"
#include "Sequence.hpp"
#include "Sequencer.hpp"
#include "FixedSequenceGroup.hpp"
#include "AlertException.hpp"
#include "WaitStrategyType.hpp"
#include "BusySpinWaitStrategy.hpp"
#include "SleepingWaitStrategy.hpp"
#include "YieldingWaitStrategy.hpp"

namespace disruptor {
    template<WaitStrategyType T>
    struct WaitStrategySelector;

    template<>
    struct WaitStrategySelector<WaitStrategyType::BUSY_SPIN> {
        using type = BusySpinWaitStrategy;
    };

    template<>
    struct WaitStrategySelector<WaitStrategyType::SLEEP> {
        using type = SleepingWaitStrategy;
    };

    template<>
    struct WaitStrategySelector<WaitStrategyType::YIELD> {
        using type = YieldingWaitStrategy;
    };

    template<WaitStrategyType T, size_t N>
    class ProcessingSequenceBarrier final : public SequenceBarrier {
    private:
        using Strategy = typename WaitStrategySelector<T>::type;
        Strategy waitStrategy;
        bool alerted;
        Sequencer &sequencer;
        FixedSequenceGroup<N> dependentSequences;

    public:
        ProcessingSequenceBarrier(
            Sequencer &sequencer,
            const WaitStrategyType waitStrategyType,
            const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences)
            : alerted(false),
              sequencer(sequencer),
              dependentSequences(dependentSequences) {
        }


        int64_t waitFor(int64_t sequence) override {
            checkAlert();

            const int64_t availableSequence = waitStrategy.waitFor(sequence, this->dependentSequences, *this);
            if (availableSequence < sequence) {
                return availableSequence;
            }

            return sequencer.getHighestPublishedSequence(sequence, availableSequence);
        }


        int64_t getCursor() const override {
            return this->dependentSequences.get();
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
    };
}
