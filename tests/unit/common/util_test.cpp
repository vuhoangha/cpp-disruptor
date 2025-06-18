#include <gtest/gtest.h>
#include "common/Util.hpp"

TEST(UtilTest, ShouldCalculateLog2) {
    ASSERT_EQ(disruptor::Util::log_2(1024), 10);
    ASSERT_EQ(disruptor::Util::log_2(1), 0);
    ASSERT_EQ(disruptor::Util::log_2(2), 1);
}

TEST(UtilTest, ShouldThrowExceptionForNonPositiveLog2) {
    ASSERT_THROW(disruptor::Util::log_2(0), std::invalid_argument);
    ASSERT_THROW(disruptor::Util::log_2(-1), std::invalid_argument);
}

TEST(UtilTest, ShouldCalculateInitialValueSequence) {
    ASSERT_EQ(disruptor::Util::calculate_initial_value_sequence(1024), 1024);
    ASSERT_EQ(disruptor::Util::calculate_initial_value_sequence(0), 0);
}

TEST(UtilTest, ShouldGetCacheLineSize) {
    // This test is basic, as the actual value is platform-dependent.
    // We just check if it returns a sane positive value.
    // Common values are 64 or 128.
    int cacheLineSize = disruptor::Util::get_cache_line_size();
    ASSERT_GT(cacheLineSize, 0);
    // Check if it's a power of two
    ASSERT_EQ((cacheLineSize & (cacheLineSize - 1)), 0);
}
