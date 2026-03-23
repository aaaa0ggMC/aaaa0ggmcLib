#ifndef ALIB5_COMPACT_MAKE_TABLE_H
#define ALIB5_COMPACT_MAKE_TABLE_H
#include <alib5/compact/manip_table.h>
#include <alib5/aperf.h>
#include <alib5/alogger.h>

namespace alib5{
    /// 支持为benchmark make table
    template<class Fn = std::nullptr_t>
    inline auto make_table(const BenchmarkResults& result,Fn fn = nullptr){
        if(result.results.empty())return log_table([](auto &op){});
        auto table = log_table([prec = result.m_precision,name = result.m_name,info = result.calculate()](auto & op){
            size_t row_index = 0;
            // 设置头颜色
            op[0][0] << LOG_COLOR1(Blue);
            op[0][1] << LOG_COLOR1(Blue);
            // 设置fmt
            std::string fmt = "{:.";
            fmt += std::to_string(prec);
            fmt += "f}";
            auto add_row = [&](auto && desc,auto && val){
                op[row_index][0] << std::forward<decltype(desc)>(desc);
                op[row_index][1] << std::forward<decltype(val)>(val);
                return op[row_index++][1];
            };
            auto add_trow = [&](auto && desc,double val_ms){
                op[row_index][0] << std::forward<decltype(desc)>(desc);
                auto p = misc::normalize_elapse(val_ms);
                op[row_index][1] << log_tfmt(fmt) << p.first << p.second;
                return op[row_index++][1];
            };

            add_row("Test",name);
            add_trow("TimeCost",info.sum);
            add_row("RunTimes",info.times);
            add_trow("Average",info.global_aver);
            add_trow("ShortestAvgCall",info.shortest_avg);
            add_trow("LongestAvgCall",info.longest_avg);
            add_row("Stddev",log_tfmt(fmt)) << info.stddev;
            add_row("CV",log_tfmt("{:.2f}")) << info.cv << "%";
        });
        table.add_restore_tag(LOG_COLOR1(None));
        table.config.col_align = ColAlign::Center;
        table.config.row_align = RowAlign::Center;
        if constexpr(requires{fn(table);}){
            fn(table);
        }
        return table;
    }
}

#endif