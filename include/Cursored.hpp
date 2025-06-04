#pragma once

namespace disruptor
{

    /**
     * Implementors of this interface must provide a single long value
     * that represents their current cursor value. Used during dynamic
     * add/remove of Sequences.
     */
    class Cursored
    {
    public:
        /**
         * Virtual destructor for proper cleanup in derived classes
         */
        virtual ~Cursored() = default;

        /**
         * Get the current cursor value.
         *
         * @return current cursor value
         */
        virtual int64_t getCursor() = 0;
    };

}