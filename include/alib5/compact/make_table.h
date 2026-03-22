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
        auto table = log_table([name = result.m_name,info = result.calculate()](auto & op){
            static auto get_time = [](double t){
                constexpr uint32_t mover_size = 4;
                constexpr const char * mover[] = {"s","ms","us","ns"}; 
                std::string ret = "";
                int mindex = 1;
                while(0 < mindex && mindex < (mover_size - 1)){
                    if(t < 1){
                        mindex += 1;
                        t *= 1000;
                    }else if(t >= 1000){
                        mindex -= 1;
                        t /= 1000;
                    }else break;
                }
                ret += std::to_string(t);
                ret += mover[mindex];
                return ret;
            };

            op[0][0] << LOG_COLOR1(Blue) << "Test" << LOG_COLOR1(None);
            op[0][1] << LOG_COLOR1(Blue) << name << LOG_COLOR1(None);
            op[1][0] << "TimeCost";
            op[1][1] << get_time(info.sum);
            op[2][0] << "RunTimes";
            op[2][1] << info.times;
            op[3][0] << "Average";
            op[3][1] << get_time(info.global_aver);
            op[4][0] << "ShortestAvgCall";
            op[4][1] << get_time(info.shortest_avg);
            op[5][0] << "LongestAvgCall";
            op[5][1] << get_time(info.longest_avg);
            op[6][0] << "Stddev";
            op[6][1] << info.stddev;
            op[7][0] << "CV";
            op[7][1] << log_tfmt("{:.2f}") << info.cv << "%";
        });
        table.config.col_align = ColAlign::Center;
        table.config.row_align = RowAlign::Center;
        if constexpr(requires{fn(table);}){
            fn(table);
        }
        return table;
    }
}

#endif