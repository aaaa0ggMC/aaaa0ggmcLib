/**
 * @file test_io.cpp
 * @brief alib6.core:io 文件读写与目录遍历单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(IoTest, ReadAndWriteAll) {
    std::string_view test_path = "gtest_io_temp.txt";
    std::string_view payload = "GoogleTest for alib6.core:io 2026";

    auto written = alib6::io::write_all(test_path, payload);
    EXPECT_EQ(written, payload.size());

    auto read_back = alib6::io::read_all(test_path);
    EXPECT_EQ(read_back, payload);

    std::filesystem::remove(test_path);
}

TEST(IoTest, TraverseDirectory) {
    alib6::io::TraverseConfig cfg;
    cfg.depth = 1;
    auto data = alib6::io::traverse_files(".", cfg);

    EXPECT_FALSE(data.targets_absolute.empty());
    EXPECT_FALSE(data.root_absolute.empty());
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(IoTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        // 测试 traverse_files 在指定 tracker 上的分配与归还
        alib6::io::TraverseConfig cfg;
        cfg.depth = 1;
        auto data = alib6::io::traverse_files(".", cfg, &tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);
        EXPECT_FALSE(data.targets_absolute.empty());
    }

    // TraverseData 析构后，所有路径字符串与 vector 必须全部释放
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
