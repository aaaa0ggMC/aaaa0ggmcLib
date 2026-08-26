/**
 * @file test_perf.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Google Test 单元测试套件：alib6.perf 基准测试、统计学指标、计时精度对照与 PMR 隔离测试
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

using namespace alib6;
using namespace alib6::perf;

// ==================== 1. 基本基准测试与优化抑制 ====================
TEST(PerfTest, BasicBenchmarkingAndOptimizationSuppression) {
    u64 accumulator = 0;

    auto results = bench("AccumulatorTest", 10'000, 3, [&] {
        accumulator += 7;
        do_not_optimize(accumulator);
    });

    EXPECT_EQ(results.results.size(), 3);
    auto info = results.calculate();
    EXPECT_EQ(info.times, 30'000);
    EXPECT_GT(info.sum, 0.0);
    EXPECT_GT(info.global_aver, 0.0);
    EXPECT_LE(info.shortest_avg, info.longest_avg);
}

// ==================== 2. 统计指标计算正确性 ====================
TEST(PerfTest, StatisticsCalculationAccuracy) {
    BenchmarkResults br("ManualStatsTest");

    // 手动填充 3 组受控数据
    // Run 1: 10.0 ms, 10 次 -> avg = 1.0 ms
    // Run 2: 20.0 ms, 10 次 -> avg = 2.0 ms
    // Run 3: 30.0 ms, 10 次 -> avg = 3.0 ms
    br.results.push_back(SingleBenchmarkResult{10.0, 10});
    br.results.push_back(SingleBenchmarkResult{20.0, 10});
    br.results.push_back(SingleBenchmarkResult{30.0, 10});

    auto info = br.calculate();

    // 总时间 60.0ms, 总次数 30, 全局均值 2.0ms
    EXPECT_DOUBLE_EQ(info.sum, 60.0);
    EXPECT_EQ(info.times, 30);
    EXPECT_DOUBLE_EQ(info.global_aver, 2.0);
    EXPECT_DOUBLE_EQ(info.shortest_avg, 1.0);
    EXPECT_DOUBLE_EQ(info.longest_avg, 3.0);

    // 标准差与变异系数应符合数学公式
    EXPECT_GT(info.stddev, 0.0);
    EXPECT_GT(info.cv, 0.0);

    // 测试字符串输出
    auto str_out = br.str();
    EXPECT_FALSE(str_out.empty());
    EXPECT_TRUE(str_out.find("ManualStatsTest") != std::string::npos);
}

// ==================== 3. 计时精度对照测试 (aperf 计时 vs 外部独立 std::chrono 对照) ====================
TEST(PerfTest, AccuracyDiscrepancyCheck) {
    constexpr u32 batch_times = 50'000;
    constexpr u32 repeat_batches = 5;

    // 工作负载：模拟一个非平凡的受控计算
    auto workload = [] {
        volatile double x = 1.0001;
        for (int i = 0; i < 50; ++i) {
            x = x * 1.00001 + 0.000001;
        }
        do_not_optimize(x);
    };

    Benchmark<decltype(workload)> benchmark(workload);

    // 执行 aperf 基准测试
    auto aperf_results = benchmark.run(batch_times, repeat_batches);
    auto aperf_info = aperf_results.calculate();

    // 外部进行独立计时比对
    double total_external_ms = 0.0;
    for (u32 b = 0; b < repeat_batches; ++b) {
        auto t0 = std::chrono::steady_clock::now();
        for (u32 i = 0; i < batch_times; ++i) {
            workload();
        }
        auto t1 = std::chrono::steady_clock::now();
        total_external_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    double diff_ms = std::abs(total_external_ms - aperf_info.sum);
    double rel_error = diff_ms / total_external_ms;

    // 打印对照精度分析结果
    std::println(
        "\n[Accuracy Report] External total: {:.4f} ms, aperf total: {:.4f} ms | Diff: {:.6f} ms (Relative Discrepancy: {:.2f}%)",
        total_external_ms, aperf_info.sum, diff_ms, rel_error * 100.0
    );

    // 由于二者测量相同的计算负载（且预热后系统进入稳态），相对误差应在合理系统抖动范围内 (通常 < 25%)
    EXPECT_LT(rel_error, 0.25);
}

// ==================== 4. 格式化与日志/输出流集成 ====================
TEST(PerfTest, FormattingAndLogIntegration) {
    auto results = bench("FormattingTest", 1000, 2, [] {
        int a = 1, b = 2;
        do_not_optimize(a + b);
    });

    // std::format
    std::string fmt_str = std::format("{}", results);
    EXPECT_TRUE(fmt_str.find("FormattingTest") != std::string::npos);

    // std::stringstream 流式输出
    std::stringstream ss;
    ss << results;
    EXPECT_TRUE(ss.str().find("FormattingTest") != std::string::npos);
}

// ==================== 5. PMR 内存池隔离与泄漏检测 Section ====================
TEST(PerfTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto results = bench(
            "PMR_Perf_Test",
            1000,
            5,
            [] {
                u32 val = 42;
                do_not_optimize(val);
            },
            &tracker
        );

        EXPECT_GT(tracker.allocated_bytes(), 0);
        auto formatted = results.str(&tracker);
        EXPECT_FALSE(formatted.empty());
    }

    // 离开作用域后，BenchmarkResults 及其内部存储必须 100% 归还，绝无泄漏
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
