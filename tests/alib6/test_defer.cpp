/**
 * @file test_defer.cpp
 * @brief alib6.core:defer 延迟执行管理器单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(DeferTest, ExecutionOrderLIFO) {
    std::vector<int> execution_log;

    {
        alib6::DeferManager dm;
        dm.defer([&]() { execution_log.push_back(1); });
        dm.defer([&]() { execution_log.push_back(2); });
        dm.defer([&]() { execution_log.push_back(3); });
        EXPECT_TRUE(execution_log.empty());
    }

    // 必须按照 LIFO (后进先出) 严格析构执行
    ASSERT_EQ(execution_log.size(), 3);
    EXPECT_EQ(execution_log[0], 3);
    EXPECT_EQ(execution_log[1], 2);
    EXPECT_EQ(execution_log[2], 1);
}

TEST(DeferTest, Clear) {
    bool executed = false;
    {
        alib6::DeferManager dm;
        dm.defer([&]() { executed = true; });
        dm.clear(); // 显式清空，析构时不应执行
    }
    EXPECT_FALSE(executed);
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(DeferTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::DeferManager dm(&tracker);
        for (int i = 0; i < 50; ++i) {
            dm.defer([i]() {
                // 做一些计算
                [[maybe_unused]] volatile int x = i * 2;
            });
        }
        EXPECT_GT(tracker.allocated_bytes(), 0);
    }

    // 退出作用域后，DeferManager 内部 pmr::deque 必须完全归还内存给 tracker
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
