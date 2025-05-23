#pragma once

#include <cstdint>
#include <concepts>

namespace disruptor
{
    /**
     * Typically used to decouple classes from RingBuffer to allow easier testing
     *
     * @tparam T The type provided by the implementation
     */
    template <typename T>
    class DataProvider
    {
    public:
        virtual ~DataProvider() = default;

        /**
         * @param sequence The sequence at which to find the data
         * @return the data item located at that sequence
         */
        [[nodiscard]] virtual T& get(int64_t sequence) = 0;
    };
}