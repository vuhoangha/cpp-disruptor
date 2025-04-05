#include <iostream>
#include <atomic>
#include "../include/Sequence.h"

int main()
{
    std::cout << "sizeof(std::atomic<int64_t>): " << sizeof(std::atomic<int64_t>) << " bytes\n";
    disruptor::Sequence sequence(0);
    std::cout << "sizeof(disruptor::Sequence): " << sizeof(disruptor::Sequence) << " bytes\n";
    return 0;
}