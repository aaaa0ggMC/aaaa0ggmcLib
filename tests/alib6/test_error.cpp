/**
 * @file test_error.cpp
 * @brief alib6.core:error 错误处理与 ErrorWrapper 范式单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(ErrorTest, RecordCollectionAndCursorClear) {
    alib6::Error err;
    EXPECT_FALSE(err.has_error());

    err.report("Error message 1");
    err.report(404, "Not Found {}", "/api/v1");

    EXPECT_TRUE(err.has_error());
    EXPECT_EQ(err.cursor, 2);

    int count = 0;
    for (const auto& rec : err) {
        if (count == 0) {
            EXPECT_EQ(rec.message, "Error message 1");
            EXPECT_EQ(rec.code, 0);
        } else if (count == 1) {
            EXPECT_EQ(rec.message, "Not Found /api/v1");
            EXPECT_EQ(rec.code, 404);
        }
        EXPECT_FALSE(rec.location.file_name() == nullptr);
        count++;
    }
    EXPECT_EQ(count, 2);

    // 测试 O(1) cursor 清空与容量保留
    err.clear();
    EXPECT_FALSE(err.has_error());
    EXPECT_EQ(err.cursor, 0);
    EXPECT_GE(err.capacity(), 2);
}

TEST(ErrorTest, WrapperPolicyFallback) {
    // 未传入 Error 时，ErrorWrapper 默认策略抛出 std::runtime_error
    auto do_fail = [](alib6::ErrorWrapper ew = {}) {
        ew.report("Operation failed unexpectedly");
    };

    EXPECT_THROW(do_fail(), std::runtime_error);

    // 传入 Error 时，平稳捕获不抛异常
    alib6::Error err;
    EXPECT_NO_THROW(do_fail(err));
    EXPECT_TRUE(err.has_error());
    EXPECT_EQ(err[0].message, "Operation failed unexpectedly");
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(ErrorTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Error err(&tracker);
        for (int i = 0; i < 20; ++i) {
            err.report(500 + i, "Simulated critical failure index: {}, long text payload beyond SSO size...", i);
        }
        EXPECT_GT(tracker.allocated_bytes(), 0);
    }

    // Error 析构后，内部 pmr::vector 与所有 pmr::string 必须彻底释放
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
