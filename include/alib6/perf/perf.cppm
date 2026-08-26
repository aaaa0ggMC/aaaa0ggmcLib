/**
 * @file perf.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 性能基准测试模块 (编译器屏障、防优化抑制、自动预热与统计分析)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cmath>
#include <limits>

export module alib6.perf;
import std;
import alib6.core;

namespace pmr = std::pmr;

export namespace alib6::perf {

    /**
     * @brief 阻止编译器优化掉目标变量 (防死代码消除 DCE)
     */
    template<typename T>
    inline __attribute__((always_inline)) void do_not_optimize(const T& value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("" : : "g"(value) : "memory");
#else
        static_cast<void>(value);
#endif
    }

    /**
     * @brief 发出编译器内存屏障，阻止指令跨行重排序
     */
    inline __attribute__((always_inline)) void clobber_memory() noexcept {
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("" : : : "memory");
#else
        // 其它平台回退
#endif
    }

    /// @brief 默认热身时长 (毫秒)
    constexpr u32 warm_up_time_ms = 100;

    /// @brief 校验基准测试函数是否可零参数调用
    template<class T>
    concept IsValidBenchmarkFunction = requires(T& t) {
        t();
    };

    /**
     * @brief 单次运行测试结果 (总耗时与迭代次数)
     */
    struct SingleBenchmarkResult {
        double timeSum{0.0}; ///< 单次运行总耗时 (毫秒)
        u32 times{0};        ///< 运行迭代总次数

        [[nodiscard]] double average() const noexcept {
            return times > 0 ? timeSum / times : 0.0;
        }

        /**
         * @brief 格式化输出为可读文本
         */
        [[nodiscard]] pmr::string str(memory_resource* mem = get_default_resource()) const {
            pmr::string buf(mem);
            std::format_to(
                std::back_inserter(buf),
                "{:>12} : {:.6f} ms\n"
                "{:>12} : {}\n"
                "{:>12} : {:.6f} ms",
                "TimeCost", timeSum,
                "RunTimes", times,
                "Average", average()
            );
            return buf;
        }

        template<class Str>
        void write_to_log(Str& s) const {
            s.append(str());
        }
    };

    /**
     * @brief 多轮次基准测试聚合结果与统计指标
     */
    struct BenchmarkResults {
        pmr::string m_name;
        pmr::vector<SingleBenchmarkResult> results;
        usize m_precision{6};

        struct CalculateInfo {
            double sum{0.0};
            double shortest_avg{std::numeric_limits<double>::max()};
            double longest_avg{0.0};
            u32 times{0};
            double stddev{0.0};
            double global_aver{0.0};
            double cv{0.0};
        };

        explicit BenchmarkResults(memory_resource* mem = get_default_resource())
            : m_name(mem), results(mem) {}

        BenchmarkResults(std::string_view name, memory_resource* mem = get_default_resource())
            : m_name(name, mem), results(mem) {}

        /**
         * @brief 计算多轮测试的综合统计指标
         */
        [[nodiscard]] CalculateInfo calculate() const noexcept {
            CalculateInfo c;
            if (results.empty()) return c;

            for (const auto& t : results) {
                c.sum += t.timeSum;
                c.times += t.times;
            }
            if (c.times == 0) return c;

            c.global_aver = c.sum / c.times;
            for (const auto& t : results) {
                double this_aver = t.times > 0 ? t.timeSum / t.times : 0.0;
                if (this_aver > c.longest_avg) c.longest_avg = this_aver;
                if (this_aver < c.shortest_avg) c.shortest_avg = this_aver;

                double diff = this_aver - c.global_aver;
                c.stddev += diff * diff * t.times;
            }

            c.stddev = std::sqrt(c.stddev / (c.times > 1 ? c.times - 1 : 1));
            c.cv = (c.global_aver != 0.0) ? (c.stddev / c.global_aver * 100.0) : 0.0;
            return c;
        }

        /**
         * @brief 格式化输出多轮完整评测报告
         */
        [[nodiscard]] pmr::string str(memory_resource* mem = get_default_resource()) const {
            if (results.empty()) return pmr::string(mem);
            auto info = calculate();

            pmr::string ss(mem);

            auto norm_format = [&](double v) {
                auto p = alib6::time::normalize_elapse(v);
                return std::format("{:.{}f} {}", p.first, m_precision, p.second);
            };

            std::format_to(
                std::back_inserter(ss),
                "\n-----------------------\n"
                "{}\n\n"
                "{:<16} : {}\n"
                "{:<16} : {}\n"
                "{:<16} : {}\n"
                "{:<16} : {}\n"
                "{:<16} : {}\n"
                "{:<16} : {:.{}f}\n"
                "{:<16} : {:.4f}%\n"
                "--------------------------------",
                m_name,
                "TimeCost", norm_format(info.sum),
                "RunTimes", info.times,
                "Average", norm_format(info.global_aver),
                "ShortestAvg", norm_format(info.shortest_avg),
                "LongestAvg", norm_format(info.longest_avg),
                "Stddev", info.stddev, m_precision,
                "CV", info.cv
            );
            return ss;
        }

        BenchmarkResults& name(std::string_view name_val) {
            m_name = name_val;
            return *this;
        }

        BenchmarkResults& precision(usize prec) noexcept {
            m_precision = prec;
            return *this;
        }

        template<class Str>
        void write_to_log(Str& s) const {
            s.append(str());
        }
    };

    /**
     * @brief 基准测试驱动器 (自动预热 + 多轮执行与数据归集)
     */
    template<IsValidBenchmarkFunction Function>
    struct Benchmark {
        Function func;

        explicit Benchmark(Function f) : func(std::move(f)) {}

        /**
         * @brief 执行基准测试
         * @param times 单轮迭代执行次数
         * @param repeat 重复测试轮数
         * @param mem 结果使用的 PMR 内存资源
         */
        BenchmarkResults run(u32 times, u32 repeat, memory_resource* mem = get_default_resource()) {
            BenchmarkResults rs(mem);

            // 1. 预热阶段：让 CPU 进入高主频睿频状态并预热 CPU 缓存
            {
                auto start = std::chrono::steady_clock::now();
                while (true) {
                    for (u32 i = 0; i < 100; ++i) {
                        if constexpr (std::is_void_v<std::invoke_result_t<Function>>) {
                            func();
                            clobber_memory();
                        } else {
                            auto r = func();
                            do_not_optimize(r);
                        }
                    }
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                    if (elapsed_ms >= warm_up_time_ms) break;
                }
            }

            // 2. 真实多轮测试阶段
            rs.results.reserve(repeat);
            for (u32 i = 0; i < repeat; ++i) {
                rs.results.push_back(single_run(times));
            }

            return rs;
        }

    private:
        SingleBenchmarkResult single_run(u32 times) {
            SingleBenchmarkResult result{0.0, times};

            auto st = std::chrono::steady_clock::now();
            for (u32 i = 0; i < times; ++i) {
                if constexpr (std::is_void_v<std::invoke_result_t<Function>>) {
                    func();
                    clobber_memory();
                } else {
                    auto res = func();
                    do_not_optimize(res);
                }
            }
            auto ed = std::chrono::steady_clock::now();

            result.timeSum = std::chrono::duration<double, std::milli>(ed - st).count();
            return result;
        }
    };

    /**
     * @brief 便捷基准测试入口函数
     */
    template<IsValidBenchmarkFunction Function>
    [[nodiscard]] BenchmarkResults bench(
        std::string_view name,
        u32 times,
        u32 repeat,
        Function&& f,
        memory_resource* mem = get_default_resource()
    ) {
        Benchmark<std::decay_t<Function>> b(std::forward<Function>(f));
        auto rs = b.run(times, repeat, mem);
        rs.name(name);
        return rs;
    }

    inline std::ostream& operator<<(std::ostream& os, const SingleBenchmarkResult& val) {
        os << val.str();
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const BenchmarkResults& val) {
        os << val.str();
        return os;
    }

} // namespace alib6::perf

export namespace alib6 {
    namespace perf = alib6::perf;
    using alib6::perf::do_not_optimize;
    using alib6::perf::clobber_memory;
    using alib6::perf::SingleBenchmarkResult;
    using alib6::perf::BenchmarkResults;
    using alib6::perf::Benchmark;
    using alib6::perf::bench;
} // namespace alib6

export namespace std {
    template<>
    struct formatter<alib6::perf::SingleBenchmarkResult> : formatter<string_view> {
        auto format(const alib6::perf::SingleBenchmarkResult& res, format_context& ctx) const {
            auto s = res.str();
            return formatter<string_view>::format(s, ctx);
        }
    };

    template<>
    struct formatter<alib6::perf::BenchmarkResults> : formatter<string_view> {
        auto format(const alib6::perf::BenchmarkResults& res, format_context& ctx) const {
            auto s = res.str();
            return formatter<string_view>::format(s, ctx);
        }
    };
} // namespace std
