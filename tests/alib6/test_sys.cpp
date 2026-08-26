/**
 * @file test_sys.cpp
 * @brief alib6.core:sys 系统信息与内存统计单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(SysTest, CpuBrand) {
    auto brand = alib6::sys::get_cpu_brand();
    EXPECT_FALSE(brand.empty());
}

TEST(SysTest, MemoryUsage) {
    auto prog_mem = alib6::sys::get_prog_mem_usage();
    EXPECT_GT(prog_mem.memory, 0); // 当前进程物理内存必定大于 0

    auto glob_mem = alib6::sys::get_global_mem_usage();
    EXPECT_GT(glob_mem.physical_total, 0); // 系统物理内存必定大于 0
    EXPECT_LE(glob_mem.percent, 100);
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(SysTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto brand = alib6::sys::get_cpu_brand(&tracker);
        EXPECT_FALSE(brand.empty());
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
