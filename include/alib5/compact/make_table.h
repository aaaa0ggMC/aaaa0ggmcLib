/**
 * @file make_table.h
 * @brief Benchmark result table generation helpers. / 基准测试结果表格生成辅助工具。
 * @author aaaa0ggmc
 * @date 2026/06/20
 * @version 5.0
 * @copyright Copyright(c) 2026
 */

#ifndef ALIB5_COMPACT_MAKE_TABLE_H
#define ALIB5_COMPACT_MAKE_TABLE_H
#include <alib5/compact/manip_table.h>
#include <alib5/aperf.h>
#include <alib5/alogger.h>

namespace alib5{
    namespace detail{
        namespace compact_mk_table{
            /// @brief Internal row data assembled from a BenchmarkResults entry for table rendering.
            struct InternalData{
                /// @brief Decimal precision used for formatting numeric fields.
                size_t prec;
                /// @brief Display name of the benchmark case.
                std::string name;
                /// @brief Pre-computed statistics (sum, average, stddev, cv, ...).
                BenchmarkResults::CalculateInfo info;
            };

            /// @brief Elapsed-time value paired with a format string, streamable into a log table.
            struct ElapseTime{
                /// @brief Raw elapsed value (in seconds).
                double v;
                /// @brief std::format-style format string, e.g. "{:.6f}".
                std::string_view fmt;

                /**
                 * @brief Forwards this value into a streamed log table context, applying magnitude normalization.
                 *
                 * @param s Target streamed context (rvalue).
                 * @return The forwarded streamed context.
                 */
                auto&& self_forward(StreamedContext<detail::LogTableMockFactory> && s){
                    auto p = misc::normalize_elapse(v);
                    std::move(s) << log_tfmt(fmt) << p.first << p.second;
                    return std::move(s);
                }
            };
        };
    };

    /**
     * @brief Builds a log table summarizing multiple BenchmarkResults, with optional row post-processing.
     *
     * @par Original Comment:
     * 支持为benchmark make table,内部使用的ForwardFn
     *
     * @tparam Fn Callback type invoked with the built table (defaults to nullptr_t).
     * @tparam ForwardFn Callback type invoked with internal row data and the table operator.
     * @param results Vector of benchmark results to render.
     * @param fn Optional callback receiving the finished table.
     * @param ifwd Optional callback invoked with (data, op) to inject extra columns/rows.
     * @return The configured log table.
     */
    template<class Fn = std::nullptr_t,class ForwardFn = std::nullptr_t>
    inline auto make_table(const std::vector<BenchmarkResults>& results,Fn&& fn = nullptr,ForwardFn && ifwd = nullptr){
        if(results.empty())return log_table([](auto &op){});
        using namespace detail::compact_mk_table;
        std::vector<InternalData> rdata;
        for(const BenchmarkResults & r : results){
            if(r.results.empty())continue;
            rdata.emplace_back(r.m_precision,r.m_name,r.calculate());
        }
        if(rdata.empty())return log_table([](auto &op){});
        auto table = log_table([fwd = std::forward<ForwardFn>(ifwd),data = std::move(rdata)](auto & op){
            size_t row_index = 0;
            // 设置头颜色
            op[0][0] << LOG_COLOR1(Blue)<< "Test";
            op[1][0] << "TimeCost";
            op[2][0] << "RunTimes";
            op[3][0] << "Average";
            op[4][0] << "ShortestAvgCall";
            op[5][0] << "LongestAvgCall";
            op[6][0] << "Stddev";
            op[7][0] << "CV";

            std::string fmt;
            for(size_t i = 1;i <= data.size();++i){
                const InternalData & d = data[i-1];
                // 处理格式化
                fmt = "{:.";
                fmt += std::to_string(d.prec);
                fmt += "f}";

                op[0][i] << LOG_COLOR1(Blue) << d.name;
                op[1][i] << ElapseTime{d.info.sum,fmt};
                op[2][i] << d.info.times;
                op[3][i] << ElapseTime{d.info.global_aver,fmt};
                op[4][i] << ElapseTime{d.info.shortest_avg,fmt};
                op[5][i] << ElapseTime{d.info.longest_avg,fmt};
                op[6][i] << log_tfmt(fmt) << d.info.stddev;
                op[7][i] << log_tfmt("{:.2f}") << d.info.cv << "%";
            }

            if constexpr(requires{fwd(data,op);}){
                fwd(data,op);
            }
        });
        table.add_restore_tag(LOG_COLOR1(None));
        table.config.col_align = ColAlign::Center;
        table.config.row_align = RowAlign::Center;
        if constexpr(requires{fn(table);}){
            fn(table);
        }
        return table;
    }

    /**
     * @brief Convenience overload that wraps a single BenchmarkResults into a one-element vector and renders it.
     *
     * @tparam Fn Callback type invoked with the built table.
     * @param result Single benchmark result to render.
     * @param fn Optional callback receiving the finished table.
     * @return The configured log table.
     */
    template<class Fn = std::nullptr_t>
    inline auto make_table(const BenchmarkResults& result,Fn&& fn = nullptr){
        std::vector<BenchmarkResults> results = {result};
        return make_table(results,std::forward<Fn>(fn));
    }

    /**
     * @brief Runs two callables under identical benchmark settings and renders a comparison table.
     *
     * @tparam Times Number of iterations per turn.
     * @tparam Turns Number of turns (each turn runs Times iterations).
     * @tparam Fn1 Type of the first callable.
     * @tparam Fn2 Type of the second callable.
     * @tparam Fn Callback type invoked with the built table.
     * @param n1 Display name for the first callable.
     * @param fn1 First callable to benchmark.
     * @param n2 Display name for the second callable.
     * @param fn2 Second callable to benchmark.
     * @param precision Decimal precision for numeric fields.
     * @param fn Optional callback receiving the finished table.
     * @return The configured comparison log table.
     */
    template<size_t Times = 1000,size_t Turns = 100,class Fn1,class Fn2,class Fn = std::nullptr_t>
    inline auto make_table_compare_bench(std::string_view n1,Fn1 && fn1,std::string_view n2,Fn2 && fn2,size_t precision = 6,Fn && fn = nullptr){
        return make_table_compare<Fn>(
            Benchmark(std::forward<Fn1>(fn1)).run(Times, Turns).name(n1),
            Benchmark(std::forward<Fn1>(fn2)).run(Times, Turns).name(n2),
            precision,
            std::forward<Fn>(fn)
        );
    }

    /**
     * @brief Renders a side-by-side comparison table for two BenchmarkResults, injecting diff rows.
     *
     * @tparam Fn Callback type invoked with the built table.
     * @param a First benchmark result.
     * @param b Second benchmark result.
     * @param precision Decimal precision for numeric fields.
     * @param fn Optional callback receiving the finished table.
     * @return The configured comparison log table.
     */
    template<class Fn = std::nullptr_t>
    inline auto make_table_compare(const BenchmarkResults& a,const BenchmarkResults & b,size_t precision = 6,Fn&& fn = nullptr){
        std::vector<BenchmarkResults> results = {a,b};
        return make_table(results,std::forward<Fn>(fn),[precision](const std::vector<detail::compact_mk_table::InternalData> & data,detail::Operator & op){
            if(data.size() < 2)return; // 数据没凑齐
            auto & a = data[0];
            auto & b = data[1];
            struct Coloring{
                /// @brief Relative difference value in [..]; sign/color derived from it.
                float value;
                /// @brief std::format-style format string for the percentage output.
                std::string_view fmt;
                /// @brief If true, inverts the green/red polarity (smaller-is-better semantics).
                bool reverse = false;

                /**
                 * @brief Forwards this coloring cell into a streamed log table context.
                 *
                 * @param ctx Target streamed context (rvalue).
                 * @return The forwarded streamed context.
                 */
                auto&& self_forward(StreamedContext<detail::LogTableMockFactory> && ctx){
                    log_tag tag = (value > 0)^reverse ? LOG_COLOR1(Green) : LOG_COLOR1(Red);
                    float out = value * 100;

                    if(value == 0)return std::move(ctx) << "-";
                    else return std::move(ctx) << tag << out << "%" << LOG_COLOR1(None);
                }
            };
            std::string fmt = "{:.";
            fmt += std::to_string(precision);
            fmt += "f}";

            op[0][3] << LOG_COLOR1(Blue) << "First Diff Based on Second";
            op[1][3] << "-";
            op[2][3] << "-";
            op[3][3] << Coloring{1 - a.info.global_aver / b.info.global_aver,fmt};
            op[4][3] << Coloring{1 - a.info.shortest_avg / b.info.shortest_avg,fmt};
            op[5][3] << Coloring{1 - a.info.longest_avg / b.info.longest_avg,fmt};
            op[6][3] << "-";
            op[7][3] << Coloring{1 - a.info.cv / b.info.cv,fmt,true};
        });
    }
}

#endif
