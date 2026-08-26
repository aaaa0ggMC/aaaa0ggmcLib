/**
 * @file test_str.cpp
 * @brief alib6.core:str 模块单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(StrTest, Trim) {
    EXPECT_EQ(alib6::str::trim(""), "");
    EXPECT_EQ(alib6::str::trim("   "), "");
    EXPECT_EQ(alib6::str::trim(" \t\r\n hello world \n\t "), "hello world");
    EXPECT_EQ(alib6::str::trim_left("  abc  "), "abc  ");
    EXPECT_EQ(alib6::str::trim_right("  abc  "), "  abc");
}

TEST(StrTest, Split) {
    auto res1 = alib6::str::split("a,b,c", ",");
    ASSERT_EQ(res1.size(), 3);
    EXPECT_EQ(res1[0], "a");
    EXPECT_EQ(res1[1], "b");
    EXPECT_EQ(res1[2], "c");

    auto res2 = alib6::str::split("hello::world::test", "::");
    ASSERT_EQ(res2.size(), 3);
    EXPECT_EQ(res2[0], "hello");
    EXPECT_EQ(res2[1], "world");
    EXPECT_EQ(res2[2], "test");

    auto res3 = alib6::str::split("single", ",");
    ASSERT_EQ(res3.size(), 1);
    EXPECT_EQ(res3[0], "single");
}

TEST(StrTest, CaseConversion) {
    EXPECT_EQ(alib6::str::to_upper("hello World 123!"), "HELLO WORLD 123!");
    EXPECT_EQ(alib6::str::to_lower("HELLO World 123!"), "hello world 123!");
}

TEST(StrTest, EscapeAndUnescape) {
    std::string_view original = "Hello \"World\"\n\t\r\\";
    auto escaped = alib6::str::escape(original);
    auto unescaped = alib6::str::unescape(escaped);
    EXPECT_EQ(unescaped, original);

    // Unicode 转义测试
    auto unicode_unescaped = alib6::str::unescape("\\u4e2d\\u6587");
    EXPECT_EQ(unicode_unescaped, "中文");
}

TEST(StrTest, StringPool) {
    alib6::str::StringPool pool;
    auto s1 = pool.get("test_key");
    auto s2 = pool.get("test_key");
    EXPECT_EQ(s1, "test_key");
    EXPECT_EQ(s1.data(), s2.data()); // 相同字符串在池内地址必须完全相同
}

TEST(ExtTest, ToStringAndToT) {
    EXPECT_EQ(alib6::ext::to_string(12345), "12345");
    EXPECT_EQ(alib6::ext::to_string(3.14), "3.14");
    EXPECT_EQ(alib6::ext::to_string("hello"), "hello");

    EXPECT_EQ(alib6::ext::to_T<int>("999"), 999);
    EXPECT_EQ(alib6::ext::to_T<double>("3.14"), 3.14);
    EXPECT_TRUE(alib6::ext::to_T<bool>("true"));
    EXPECT_FALSE(alib6::ext::to_T<bool>("false"));
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(StrTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        // 1. 测试 split PMR 内存隔离
        auto vec = alib6::str::split("one,two,three,four,five", ",", &tracker);
        EXPECT_GT(tracker.allocation_count(), 0);
        EXPECT_EQ(vec.size(), 5);
    }
    // 离开作用域后，vector 必须彻底释放内存
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);

    {
        // 2. 测试 escape / unescape PMR 内存隔离
        auto escaped = alib6::str::escape("PMR Test Long String With Escapes \n\t\r", false, &tracker);
        auto unescaped = alib6::str::unescape(escaped, &tracker);
        EXPECT_EQ(unescaped, "PMR Test Long String With Escapes \n\t\r");
    }
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);

    {
        // 3. 测试 StringPool 内存归属与彻底析构释放
        {
            alib6::str::StringPool pool(&tracker);
            [[maybe_unused]] auto s1 = pool.get("A long string exceeding small string optimization size 1234567890");
            [[maybe_unused]] auto s2 = pool.get("Another long string exceeding SSO buffer 1234567890");
            EXPECT_GT(tracker.allocated_bytes(), 0);
        }
        // 池析构后，所有驻留字符串内存必须彻底释放
        EXPECT_FALSE(tracker.has_leak());
        EXPECT_EQ(tracker.live_bytes(), 0);
    }
}
