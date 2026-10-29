#include <gtest/gtest.h>
#include "GpuTiming.h"
#include <cstdint>

TEST(GpuTimingTest, TicksToMilliseconds) {
    EXPECT_DOUBLE_EQ(TimestampMilliseconds(1000, 3'001'000, 64, 1.0), 3.0);
    EXPECT_DOUBLE_EQ(TimestampMilliseconds(0, 1'000'000, 64, 2.5), 2.5);
}

TEST(GpuTimingTest, CounterWrapsAroundValidBits) {
    const uint64_t last = (uint64_t{1} << 36) - 500'000;
    EXPECT_DOUBLE_EQ(TimestampMilliseconds(last, 500'000, 36, 1.0), 1.0);
}

TEST(GpuTimingTest, SummaryOfOddCount) {
    const TimingSummary summary = Summarize({5.0, 1.0, 3.0});
    EXPECT_DOUBLE_EQ(summary.median, 3.0);
    EXPECT_DOUBLE_EQ(summary.percentile95, 5.0);
}

TEST(GpuTimingTest, SummaryOfEvenCountAveragesTheMiddle) {
    EXPECT_DOUBLE_EQ(Summarize({4.0, 1.0, 2.0, 3.0}).median, 2.5);
}

TEST(GpuTimingTest, Percentile95IsNearestRank) {
    std::vector<double> durations;
    for (int i = 1; i <= 100; ++i) durations.push_back(i);
    EXPECT_DOUBLE_EQ(Summarize(durations).percentile95, 95.0);
}

TEST(GpuTimingTest, EmptySummaryIsZero) {
    const TimingSummary summary = Summarize({});
    EXPECT_DOUBLE_EQ(summary.median, 0.0);
    EXPECT_DOUBLE_EQ(summary.percentile95, 0.0);
}
