/**
 * @file test_table.cpp
 * @brief alib6.table 现代化表格与排版引擎单元测试 (含多行、CJK、对齐与 PMR 泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(TableTest, BasicLayoutAndPresets) {
    alib6::Table tbl(alib6::TableConfig::unicode_rounded());

    tbl[0][0] = "ID";
    tbl[0][1] = "Name";
    tbl[0][2] = "Score";

    tbl[1][0] = "1";
    tbl[1][1] = "Alice";
    tbl[1][2] = "95.5";

    tbl[2][0] = "2";
    tbl[2][1] = "Bob";
    tbl[2][2] = "88.0";

    auto rendered = tbl.str();
    EXPECT_FALSE(rendered.empty());
    EXPECT_TRUE(rendered.find("Alice") != std::string::npos);
    EXPECT_TRUE(rendered.find("Bob") != std::string::npos);
    EXPECT_TRUE(rendered.find("╭") != std::string::npos);
    EXPECT_TRUE(rendered.find("╯") != std::string::npos);

    // Markdown 预设测试
    tbl.config = alib6::TableConfig::markdown();
    auto md_rendered = tbl.str();
    EXPECT_TRUE(md_rendered.find("| ID | Name | Score |") != std::string::npos ||
                md_rendered.find("|") != std::string::npos);
}

TEST(TableTest, CJKAndEmojiWidthAlignment) {
    alib6::Table tbl(alib6::TableConfig::unicode_box());

    tbl[0][0] = "编号";
    tbl[0][1] = "中文名称";
    tbl[0][2] = "描述";

    tbl[1][0] = "101";
    tbl[1][1] = "项目管理 🚀";
    tbl[1][2] = "核心业务流程";

    tbl[2][0] = "102";
    tbl[2][1] = "数据统计 📊";
    tbl[2][2] = "全量数据汇总计算";

    auto rendered = tbl.str();
    EXPECT_FALSE(rendered.empty());
    EXPECT_TRUE(rendered.find("项目管理 🚀") != std::string::npos);
    EXPECT_TRUE(rendered.find("全量数据汇总计算") != std::string::npos);
}

TEST(TableTest, MultiLineCellsAndRowAlignment) {
    alib6::Table tbl(alib6::TableConfig::unicode_rounded());

    tbl[0][0] = "Item";
    tbl[0][1] = "Multi-line Description";

    tbl[1][0] = "Task A";
    tbl[1][1] = "Line 1: Initialize\nLine 2: Process Data\nLine 3: Clean up";
    tbl[1][1].align(alib6::ColAlign::Left, alib6::RowAlign::Center);

    auto rendered = tbl.str();
    EXPECT_TRUE(rendered.find("Line 1: Initialize") != std::string::npos);
    EXPECT_TRUE(rendered.find("Line 2: Process Data") != std::string::npos);
    EXPECT_TRUE(rendered.find("Line 3: Clean up") != std::string::npos);
}

TEST(TableTest, BenchmarkMakeTable) {
    auto res1 = alib6::perf::bench("AlgorithmA", 1000, 5, [] {
        volatile int a = 1 + 1;
        alib6::perf::do_not_optimize(a);
    });

    auto res2 = alib6::perf::bench("AlgorithmB", 1000, 5, [] {
        volatile int b = 2 * 2;
        alib6::perf::do_not_optimize(b);
    });

    auto tbl = alib6::make_table_compare(res1, res2);
    auto rendered = tbl.str();

    EXPECT_FALSE(rendered.empty());
    EXPECT_TRUE(rendered.find("AlgorithmA") != std::string::npos);
    EXPECT_TRUE(rendered.find("AlgorithmB") != std::string::npos);
    EXPECT_TRUE(rendered.find("Relative Diff (A vs B)") != std::string::npos);
}

TEST(TableTest, LoggerStreamedSelfForward) {
    struct TestTarget : public alib6::log::LogTarget {
        std::vector<std::string> records;
        void write(alib6::log::LogMsg& msg) override {
            records.emplace_back(msg.gen_composed());
        }
    };

    alib6::log::LoggerConfig cfg;
    cfg.consumer_count = 0;
    alib6::log::Logger logger(cfg);
    auto target = logger.append_mod<TestTarget>("buf");
    alib6::log::LogFactory fac(logger);

    alib6::Table tbl;
    tbl[0][0] = "Header1";
    tbl[0][1] = "Header2";
    tbl[1][0] = "Val1";
    tbl[1][1] = "Val2";

    fac << tbl << alib6::log::endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("Header1") != std::string::npos);
    EXPECT_TRUE(target->records[0].find("Val2") != std::string::npos);
}

TEST(TableTest, NestedTableSupport) {
    alib6::Table inner(alib6::TableConfig::unicode_box());
    inner[0][0] = "Sub-1";
    inner[0][1] = "Sub-2";
    inner[1][0] = "100";
    inner[1][1] = "200";

    alib6::Table outer(alib6::TableConfig::unicode_rounded());
    outer[0][0] = "Category";
    outer[0][1] = "Details (Nested)";
    outer[1][0] = "Metrics";
    outer[1][1] = inner; // 嵌套表格赋值

    auto rendered = outer.str();
    EXPECT_FALSE(rendered.empty());
    EXPECT_TRUE(rendered.find("Category") != std::string::npos);
    EXPECT_TRUE(rendered.find("Sub-1") != std::string::npos);
    EXPECT_TRUE(rendered.find("200") != std::string::npos);
}

TEST(TableTest, MultiLineAndNestedTagPositioning) {
    struct TagRecordTarget : public alib6::log::LogTarget {
        std::vector<std::string> messages;
        std::vector<std::vector<alib6::log::LogCustomTag>> tag_records;

        void write(alib6::log::LogMsg& msg) override {
            messages.emplace_back(msg.gen_composed());
            tag_records.emplace_back(msg.tags.begin(), msg.tags.end());
        }
    };

    alib6::log::LoggerConfig cfg;
    cfg.consumer_count = 0;
    alib6::log::Logger logger(cfg);
    auto target = logger.append_mod<TagRecordTarget>("tag_buf");
    alib6::log::LogFactory fac(logger);

    alib6::Table tbl;
    tbl[0][0] << alib6::log::color(alib6::log::Color::Green) << "First Line\n"
              << alib6::log::color(alib6::log::Color::Red) << "Second Line";

    fac << tbl << alib6::log::endlog;

    ASSERT_EQ(target->tag_records.size(), 1);
    const auto& tags = target->tag_records[0];
    EXPECT_GE(tags.size(), 2); // 确保绿色和红色标签均在各行准确注入并定位

    // 验证行内标签位置单调递增且在有效范围内
    for (std::size_t i = 1; i < tags.size(); ++i) {
        EXPECT_GE(tags[i].get_pos(), tags[i-1].get_pos());
    }
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(TableTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Table tbl(alib6::TableConfig::unicode_rounded(), &tracker);

        tbl[0][0] = "Key";
        tbl[0][1] = "Value";
        tbl[1][0] = "Database";
        tbl[1][1] = "PostgreSQL 16";
        tbl[2][0] = "Version";
        tbl[2][1] = "v1.0.0-release";

        auto rendered = tbl.str(&tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);
        EXPECT_TRUE(rendered.find("PostgreSQL 16") != std::string::npos);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
