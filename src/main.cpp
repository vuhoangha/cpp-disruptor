#include <iostream>
#include <atomic>
#include "../include/Sequence.hpp"
#include "../include/Util.hpp"

int main()
{
    int cache_line = disruptor::Util::get_cache_line_size();
    if (cache_line != 64)
    {
        std::cout << "cache line invalid: " << cache_line << " bytes\n";
    }

    std::cout << "sizeof(std::atomic<int64_t>): " << sizeof(std::atomic<int64_t>) << " bytes\n";
    disruptor::Sequence sequence(0);
    std::cout << "sizeof(disruptor::Sequence): " << sizeof(disruptor::Sequence) << " bytes\n";

    return 0;
}