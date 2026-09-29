/**
 * @file test_log.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Google Test 单元测试套件：alib6.log 与 alib6.log.prefab 全功能、Move 传递与 PMR 隔离测试
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
#include <alib6/log/glm_ext.h>
import std;
import alib6;

using namespace alib6;
using namespace alib6::log;

// 自定义测试输出目标 (收集所有写入的字符串)
struct TestStringTarget : public LogTarget {
    std::mutex mtx;
    std::vector<std::string> records;

    void write(LogMsg& msg) override {
        std::lock_guard<std::mutex> lk(mtx);
        records.emplace_back(msg.gen_composed());
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx);
        records.clear();
    }
};

// ==================== 1. LogMsg 移动语义与生命周期 ====================
TEST(LogTest, MessageMoveSemantics) {
    alib6::test::CountingMemoryResource tracker;

    {
        std::pmr::string original_body("Zero-copy move semantics test body text", &tracker);
        const char* original_data_ptr = original_body.data();

        LogMsg msg(&tracker, &tracker);
        msg.body = std::move(original_body);
        msg.level = static_cast<int>(LogLevel::Warn);

        // 验证移动后数据指针未发生二次拷贝
        EXPECT_EQ(msg.body.data(), original_data_ptr);
        EXPECT_TRUE(original_body.empty());

        // 测试 LogMsg 自身的移动构造
        LogMsg moved_msg(std::move(msg));
        EXPECT_EQ(moved_msg.body.data(), original_data_ptr);
        EXPECT_EQ(moved_msg.level, static_cast<int>(LogLevel::Warn));
    }

    EXPECT_FALSE(tracker.has_leak());
}

// ==================== 2. Logger 与 LogFactory 同步基本写入 ====================
TEST(LogTest, SyncLoggingAndFormatting) {
    LoggerConfig cfg;
    cfg.consumer_count = 0; // 同步模式

    Logger logger(cfg);
    auto target = logger.append_mod<TestStringTarget>("test_target");

    LogFactory fac(logger, "TestModule");
    fac.log(LogLevel::Info, "Hello {}", "World");
    fac.log(LogLevel::Error, "Error code: {:04d}", 404);

    ASSERT_EQ(target->records.size(), 2);
    EXPECT_TRUE(target->records[0].find("Hello World") != std::string::npos);
    EXPECT_TRUE(target->records[0].find("[INFO]") != std::string::npos);
    EXPECT_TRUE(target->records[0].find("[TestModule]") != std::string::npos);

    EXPECT_TRUE(target->records[1].find("Error code: 0404") != std::string::npos);
    EXPECT_TRUE(target->records[1].find("[ERROR]") != std::string::npos);
}

// ==================== 3. 流式上下文 StreamedContext 与操作符链 ====================
TEST(LogTest, StreamedContextAndManipulators) {
    LoggerConfig cfg;
    cfg.consumer_count = 0;

    Logger logger(cfg);
    auto target = logger.append_mod<TestStringTarget>("test_target");

    LogFactory fac(logger, "Stream");

    // 基础流式输出
    fac << "Score: " << 100 << " Points!" << endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("Score: 100 Points!") != std::string::npos);

    target->clear();

    // 测试 log_fmt 与 log_tfmt
    fac << log_tfmt("{:08x}") << 255 << " normal " << 42 << fls;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("000000ff normal 42") != std::string::npos);

    target->clear();

    // 测试 log_erase
    fac << "Hello World12345" << log_erase(5) << endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("Hello World") != std::string::npos);
    EXPECT_TRUE(target->records[0].find("12345") == std::string::npos);

    target->clear();

    // 测试 log_omit 截断
    std::string long_str(100, 'A');
    fac << log_omit(long_str, 10, "...[cut]") << endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("AAAAAAAAAA...[cut]") != std::string::npos);

    target->clear();

    // 测试 log_bin
    u8 bin_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    fac << "Hex: " << log_bin(bin_data, sizeof(bin_data)) << endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("de ad be ef") != std::string::npos);

    target->clear();

    // 测试 couple
    fac << "Combined: " << couple(1, 2, "three") << endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("12three") != std::string::npos);
}

// ==================== 4. 过滤器与拦截测试 ====================
TEST(LogTest, FilterAndBlocker) {
    LoggerConfig cfg;
    cfg.consumer_count = 0;

    Logger logger(cfg);
    auto target = logger.append_mod<TestStringTarget>("target");

    // 添加级别过滤器：拦截低于 Warn 的消息
    logger.append_mod<MinLevelFilter>("level_filter", LogLevel::Warn);

    LogFactory fac(logger, "FilterTest");
    fac.log(LogLevel::Debug, "This should be blocked");
    fac.log(LogLevel::Info, "This should also be blocked");
    fac.log(LogLevel::Warn, "Warn passed");
    fac.log(LogLevel::Error, "Error passed");

    ASSERT_EQ(target->records.size(), 2);
    EXPECT_TRUE(target->records[0].find("Warn passed") != std::string::npos);
    EXPECT_TRUE(target->records[1].find("Error passed") != std::string::npos);
}

// ==================== 5. 异步多线程消费模式 ====================
TEST(LogTest, AsyncConsumerLogging) {
    LoggerConfig cfg;
    cfg.consumer_count = 1; // 启用后台消费者线程
    cfg.fetch_message_count_max = 64;

    Logger logger(cfg);
    auto target = logger.append_mod<TestStringTarget>("async_target");

    LogFactory fac(logger, "AsyncTest");

    constexpr int msg_count = 100;
    for (int i = 0; i < msg_count; ++i) {
        fac.log(LogLevel::Info, "Async Message #{}", i);
    }

    // 刷新等待队列全部消费完毕
    logger.flush();

    ASSERT_EQ(target->records.size(), msg_count);
    EXPECT_TRUE(target->records[0].find("Async Message #0") != std::string::npos);
    EXPECT_TRUE(target->records[msg_count - 1].find("Async Message #99") != std::string::npos);
}

// ==================== 6. 开箱即用预制件 aout 与 Tag 合并测试 ====================
TEST(LogTest, PrefabAout) {
    // 验证 aout 流式操作符与 flush 不发生崩溃
    aout << "Prefab aout test log content" << endlog;
    aout << "Another line with int: " << 12345 << fls;
}

TEST(LogTest, CompactColorTagsAndMergeColorTags) {
    std::pmr::vector<alib6::log::LogCustomTag> tags;

    // 乱序推入多个颜色 tags，包含同位置覆盖
    alib6::log::LogCustomTag tag1(0, 31, 10); // pos 10, cat 0, payload 31 (Red)
    alib6::log::LogCustomTag tag2(0, 32, 5);  // pos 5,  cat 0, payload 32 (Green)
    alib6::log::LogCustomTag tag3(0, 0, 10);  // pos 10, cat 0, payload 0 (None) -> 应该覆写 tag1

    tags.push_back(tag1);
    tags.push_back(tag2);
    tags.push_back(tag3);

    alib6::log::compact_color_tags(tags);

    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0].get_pos(), 5);
    EXPECT_EQ(tags[0].payload, 32);
    EXPECT_EQ(tags[1].get_pos(), 10);
    EXPECT_EQ(tags[1].payload, 0); // 确认覆盖为最新的 payload

    // 线性 merge_color_tag 测试
    usize slot = 0;
    alib6::log::LogCustomTag incoming(0, 33, 7); // pos 7, cat 0, payload 33 (Yellow)
    alib6::log::merge_color_tag(incoming, tags, slot);

    ASSERT_EQ(tags.size(), 3);
    EXPECT_EQ(tags[0].get_pos(), 5);
    EXPECT_EQ(tags[1].get_pos(), 7);
    EXPECT_EQ(tags[1].payload, 33);
    EXPECT_EQ(tags[2].get_pos(), 10);
}

// ==================== 7. fastfmt 非侵入式高性能格式化测试 ====================
TEST(LogTest, FastFmtTypes) {
    LoggerConfig cfg;
    cfg.consumer_count = 0;
    Logger logger(cfg);
    auto target = logger.append_mod<TestStringTarget>("fastfmt_target");
    LogFactory fac(logger, "FastFmtTest");

    // nullptr & 指针
    int val = 42;
    int* ptr = &val;
    fac << "ptr=" << ptr << ", null=" << nullptr << endlog;
    EXPECT_TRUE(target->records.back().find("ptr=0x") != std::string::npos);
    EXPECT_TRUE(target->records.back().find("null=nullptr") != std::string::npos);

    // std::chrono::duration
    using namespace std::chrono_literals;
    fac << "duration=" << 150ms << ", " << 20us << ", " << 3s << endlog;
    EXPECT_TRUE(target->records.back().find("duration=150ms, 20us, 3s") != std::string::npos);

    // std::optional & std::pair & std::variant
    std::optional<int> opt_val = 100;
    std::optional<int> opt_null = std::nullopt;
    std::pair<std::string, int> pair_val{"key", 777};
    std::variant<std::monostate, int, double> var_val = 3.14;

    fac << "opt=" << opt_val << ", opt_null=" << opt_null << ", pair=" << pair_val << ", var=" << var_val << endlog;
    EXPECT_TRUE(target->records.back().find("opt=100") != std::string::npos);
    EXPECT_TRUE(target->records.back().find("opt_null=nullopt") != std::string::npos);
    EXPECT_TRUE(target->records.back().find("pair=(key, 777)") != std::string::npos);

    // GLM 向量 (若支持)
    #if defined(GLM_ENABLE_EXPERIMENTAL) || defined(GLM_VERSION)
    glm::vec3 pos(1.0f, 2.5f, -3.0f);
    fac << "pos=" << pos << endlog;
    EXPECT_TRUE(target->records.back().find("pos=vec3(1, 2.5, -3)") != std::string::npos);
    #endif
}

// ==================== 8. PMR 内存池隔离与泄漏检测 Section ====================
TEST(LogTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        LoggerConfig cfg;
        cfg.consumer_count = 0; // 同步测试内存完全归还
        Logger logger(cfg, &tracker);

        EXPECT_GT(tracker.allocated_bytes(), 0);

        auto target = logger.append_mod<TestStringTarget>("pmr_target");
        LogFactory fac(logger, "PMRModule");

        for (int i = 0; i < 50; ++i) {
            fac.log(LogLevel::Info, "PMR Allocation Log Item Index {}", i);
            fac << "Streamed PMR Item: " << i * 10 << endlog;
        }

        EXPECT_EQ(target->records.size(), 100);
        logger.flush();
    }

    // 离开作用域后，Logger、映射表、常量池等所有分配内存必须 100% 归还，绝无泄漏
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
