#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <thread>
#include <chrono>
#include <mutex>
#include "Sequence.h"

namespace disruptor
{
    class Util
    {
    public:
        static constexpr int ONE_MILLISECOND_IN_NANOSECONDS = 1'000'000;

        static int ceilingNextPowerOfTwo(int x)
        {
            return 1 << (32 - __builtin_clz(x - 1));
        }

        static int64_t getMinimumSequence(const std::vector<Sequence> &sequences, int64_t minimum = INT64_MAX)
        {
            int64_t minimumSequence = minimum;
            for (const auto &sequence : sequences)
            {
                int64_t value = sequence.get();
                minimumSequence = std::min(minimumSequence, value);
            }
            return minimumSequence;
        }

        static std::vector<Sequence> getSequencesFor(const std::vector<EventProcessor> &processors)
        {
            std::vector<Sequence> sequences;
            for (const auto &processor : processors)
            {
                sequences.push_back(processor.getSequence());
            }
            return sequences;
        }

        static int log2(int value)
        {
            if (value < 1)
            {
                throw std::invalid_argument("value must be a positive number");
            }
            return 31 - __builtin_clz(value);
        }

        static int64_t awaitNanos(std::mutex &mutex, int64_t timeoutNanos)
        {
            std::unique_lock<std::mutex> lock(mutex);
            auto start = std::chrono::steady_clock::now();
            auto duration = std::chrono::nanoseconds(timeoutNanos);
            std::cv_status status = std::cv_status::no_timeout;
            if (duration.count() > 0)
            {
                status = std::cv_status::timeout;
                std::this_thread::sleep_for(duration);
            }
            auto end = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }
    };
}