/**
 * @file test_time.cpp
 * @brief alib6.core:time 时间格式化与耗时换算单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(TimeTest, GetTimeFormat) {
    auto t = alib6::time::get_time();
    // 检查格式是否为 YYYY-MM-DD HH:MM:SS
    EXPECT_EQ(t.size(), 19);
    EXPECT_EQ(t[4], '-');
    EXPECT_EQ(t[7], '-');
    EXPECT_EQ(t[10], ' ');
    EXPECT_EQ(t[13], ':');
    EXPECT_EQ(t[16], ':');
}

TEST(TimeTest, FormatDuration) {
    EXPECT_EQ(alib6::time::format_duration(0), "0s");
    EXPECT_EQ(alib6::time::format_duration(45), "45s");
    EXPECT_EQ(alib6::time::format_duration(125), "2m 5s");
    EXPECT_EQ(alib6::time::format_duration(3665), "1h 1m 5s");
    EXPECT_EQ(alib6::time::format_duration(86400 * 365 + 3600 * 2 + 10), "1y 2h 10s");
}

TEST(TimeTest, NormalizeElapse) {
    auto [t1, u1] = alib6::time::normalize_elapse(1500.0); // 1500 ms -> 1.5 s
    EXPECT_NEAR(t1, 1.5, 0.001);
    EXPECT_EQ(u1, "s");

    auto [t2, u2] = alib6::time::normalize_elapse(0.005); // 0.005 ms -> 5 us
    EXPECT_NEAR(t2, 5.0, 0.001);
    EXPECT_EQ(u2, "us");
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(TimeTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto t_str = alib6::time::get_time(&tracker);
        auto dur_str = alib6::time::format_duration(86400 * 500 + 3600 * 12 + 45, &tracker);
        EXPECT_FALSE(t_str.empty());
        EXPECT_FALSE(dur_str.empty());
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
