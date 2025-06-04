#pragma once

enum class WaitStrategyType
{
    SLEEP,
    BUSY_SPIN,
    YIELD,
};
