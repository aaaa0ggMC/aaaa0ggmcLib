/**
 * @file test_parser.cpp
 * @brief alib6.core:parser 命令行分词与渐进式事务分析器单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(ParserTest, BasicParsing) {
    alib6::Parser parser;
    parser.parse("git commit -m \"initial commit\" --author=test");

    EXPECT_EQ(parser.head, "git");
    ASSERT_EQ(parser.args.size(), 5);
    EXPECT_EQ(parser.args[0], "git");
    EXPECT_EQ(parser.args[1], "commit");
    EXPECT_EQ(parser.args[2], "-m");
    EXPECT_EQ(parser.args[3], "initial commit");
    EXPECT_EQ(parser.args[4], "--author=test");

    EXPECT_TRUE(parser.match("git"));
    EXPECT_TRUE(parser.match({"git", "svn"}));
    EXPECT_FALSE(parser.match("hg"));
}

TEST(ParserTest, ValueCastAndExpect) {
    alib6::Parser::Value v_int("12345");
    EXPECT_TRUE(v_int);
    EXPECT_EQ(v_int.as<int>(), 12345);
    EXPECT_EQ(v_int.value_or(0), 12345);
    EXPECT_EQ(v_int.expect<int>(), 12345);

    alib6::Parser::Value v_invalid(false);
    EXPECT_FALSE(v_invalid);
    EXPECT_EQ(v_invalid.value_or(999), 999);
    EXPECT_EQ(v_invalid.expect<int>(), std::nullopt);
}

TEST(ParserTest, AnalyserAndCursorTransactions) {
    alib6::Parser parser;
    parser.parse("server --port 8080 --host=127.0.0.1 --verbose file1.txt file2.txt");

    auto ana = parser.analyse();

    // 提取 option
    auto port_val = ana.extract_an_option("--port", "");
    EXPECT_EQ(port_val.view(), "8080");

    auto host_val = ana.extract_an_option("--host", "=");
    EXPECT_EQ(host_val.view(), "127.0.0.1");

    // 提取 flag
    EXPECT_TRUE(ana.extract_flag("--verbose"));
    EXPECT_FALSE(ana.extract_flag("--debug"));

    // 检查剩余位置参数
    auto remains = ana.remains();
    ASSERT_EQ(remains.size(), 2);
    EXPECT_EQ(remains[0], "file1.txt");
    EXPECT_EQ(remains[1], "file2.txt");
}

TEST(ParserTest, PipeAnalysis) {
    alib6::Parser parser;
    parser.parse("cat file.txt | grep error | wc -l");

    auto segments = parser.analyse_pipe();
    ASSERT_EQ(segments.size(), 3);
    EXPECT_EQ(segments[0].inputs[0], "cat");
    EXPECT_EQ(segments[1].inputs[0], "grep");
    EXPECT_EQ(segments[2].inputs[0], "wc");
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(ParserTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Parser parser(&tracker);
        parser.parse("curl -X POST https://api.example.com/v1/resource -d \"{\\\"key\\\": \\\"value\\\"}\"", true);
        EXPECT_GT(tracker.allocated_bytes(), 0);

        auto ana = parser.analyse();
        auto method = ana.extract_an_option("-X", "");
        EXPECT_EQ(method.view(), "POST");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
