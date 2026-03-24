#ifndef ALIB5_COMPACT_MAKE_TABLE_H
#define ALIB5_COMPACT_MAKE_TABLE_H
#include <alib5/compact/manip_table.h>
#include <alib5/aperf.h>
#include <alib5/alogger.h>

namespace alib5{
    namespace detail{
        namespace compact_mk_table{
            struct InternalData{
                size_t prec;
                std::string name;
                BenchmarkResults::CalculateInfo info;
            };

            struct ElapseTime{
                double v;
                std::string_view fmt;

                auto&& self_forward(StreamedContext<detail::LogTableMockFactory> && s){
                    auto p = misc::normalize_elapse(v);
                    std::move(s) << log_tfmt(fmt) << p.first << p.second;
                    return std::move(s);
                }
            };
        };
    };

    /// 支持为benchmark make table,内部使用的ForwardFn
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
    template<class Fn = std::nullptr_t>
    inline auto make_table(const BenchmarkResults& result,Fn&& fn = nullptr){
        std::vector<BenchmarkResults> results = {result};
        return make_table(results,std::forward<Fn>(fn));
    }

    template<class Fn = std::nullptr_t>
    inline auto make_table_compare(const BenchmarkResults& a,const BenchmarkResults & b,size_t precision = 6,Fn&& fn = nullptr){
        std::vector<BenchmarkResults> results = {a,b};
        return make_table(results,std::forward<Fn>(fn),[precision](const std::vector<detail::compact_mk_table::InternalData> & data,detail::Operator & op){
            if(data.size() < 2)return; // 数据没凑齐
            auto & a = data[0];
            auto & b = data[1];
            struct Coloring{
                float value;
                std::string_view fmt;
                bool reverse = false;

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