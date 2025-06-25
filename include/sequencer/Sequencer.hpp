#pragma once

#include "../sequence/Sequence.hpp"

namespace disruptor {
    class Sequencer {
    public:
        /**
         * Virtual destructor for proper cleanup in derived classes
         */
        virtual ~Sequencer() = default;

        [[nodiscard]] virtual bool is_available(size_t sequence) const = 0;

        virtual void add_gating_sequences(std::initializer_list<std::reference_wrapper<Sequence> > sequences) = 0;

        [[nodiscard]] virtual size_t get_highest_published_sequence(size_t next_sequence, size_t available_sequence) const = 0;

        virtual size_t next(size_t n) = 0;

        virtual void publish(size_t sequence) = 0;

        virtual void publish(size_t lo, size_t hi) = 0;
    };
}
