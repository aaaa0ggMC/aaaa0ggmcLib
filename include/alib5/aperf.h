/**
 * @file aperf.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一个简单的性能计算库，能确保数据大致准确，同时省的我每次都要写差不多的benchmark代码
 * @version 5.0
 * @date 2026/03/23
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/08 
 */
#ifndef ALIB5_APERF
#define ALIB5_APERF
#include <alib5/aclock.h>
#include <sstream>
#include <iomanip>
#include <cmath>

#ifndef ALIB_PERF_CLOCK_SOURCE
#ifndef CLOCK_MONOTONIC_RAW
#ifndef CLOCK_REALTIME_COARSE
#define ALIB_PERF_CLOCK_SOURCE CLOCK_REALTIME
#else
#define ALIB_PERF_CLOCK_SOURCE CLOCK_REALTIME_COARSE
#endif
#else 
#define ALIB_PERF_CLOCK_SOURCE CLOCK_MONOTONIC_RAW
#endif 
#endif

namespace alib5{
    /// @brief 进行热身的每轮次数
    constexpr uint32_t warm_up_per_run_times = 1'0000;
    /// @brief 最低热身时长
    constexpr uint32_t warm_up_time_ms = 100;
    
    template<class T> concept IsValidBenchmarkFunction = requires(T & t){
        t();
    };

    /// 单次运行结果
    struct SingleBenchmarkResult{
        double timeSum; ///< 单次运行总时间
        uint32_t times; ///< 运行总次数

        // 由于benchmark只会出现在debug下，不为性能考虑
        /// 输出为string
        inline std::string str() const{
            std::stringstream ss;
            ss << std::setw(9) << "TimeCost" << ":" << timeSum << "ms\n"
               << std::setw(9) << "RunTimes" << ":" << times << "\n"
               << std::setw(9) << "Average"  << ":" << timeSum/times;
            return ss.str();
        }

        template<class Str> inline void write_to_log(Str & s){
            s.append(str());
        }
    };

    /// Repeat结果
    struct BenchmarkResults{
        std::string m_name;
        /// 结果列表
        std::vector<SingleBenchmarkResult> results;
        /// 除了CV外精度
        size_t m_precision { 6 };

        struct CalculateInfo{
            double sum = 0;
            double shortest_avg = __DBL_MAX__,longest_avg = 0;
            uint32_t times = 0;
            double stddev = 0;
            double global_aver = 0;
            double cv = 0;
        };

        CalculateInfo calculate() const {
            CalculateInfo c;

            for(auto & t : results){
                c.sum += t.timeSum;
                c.times += t.times;
            }
            if(!c.times)return c;
            c.global_aver = c.sum / c.times;
            for(auto & t : results){
                double thisAver = t.timeSum / t.times;

                if(thisAver > c.longest_avg)c.longest_avg = thisAver;
                else if(thisAver < c.shortest_avg)c.shortest_avg = thisAver;

                c.stddev += (thisAver - c.global_aver)*(thisAver-c.global_aver) * t.times;
            }
            c.stddev = std::sqrt(c.stddev / (c.times - 1));
            c.cv = c.stddev / c.global_aver * 100;
            return c;
        }

        // 由于benchmark只会出现在debug下，不为性能考虑
        /// 输出为string
        inline std::string str() const{
            if(results.empty())return "";
            auto info = calculate();


            std::stringstream ss;
            auto norm_insert = [&](double v) -> auto& {
                auto p = misc::normalize_elapse(v);
                ss << std::setprecision(m_precision) <<  p.first << p.second;
                return ss;
            };

            ss << "\n";
            ss << "-----------------------" << "\n";
            ss << m_name << "\n\n";
            ss << std::setprecision(8) << std::left;
            ss << std::setw(16) << "TimeCost" << ":";
            norm_insert(info.sum) << "\n"
               << std::setw(16) << "RunTimes" << ":" << info.times << "\n"
               << std::setw(16) << "Average"  << ":";
            norm_insert(info.global_aver) << "\n"
               << std::setw(16) << "ShortestAvgCall" << ":";
            norm_insert(info.shortest_avg) << "\n"
               << std::setw(16) << "LongestAvgCall"  << ":";
            norm_insert(info.longest_avg) << "\n"
               << std::setw(16) << "Stddev"  << ":" << std::setprecision(m_precision) << info.stddev << "\n"
               << std::setw(16) << "CV"  << ":" << std::setprecision(4) << info.cv << "%";
            ss << "\n--------------------------------";
            return ss.str();
        }

        BenchmarkResults& name(std::string name){
            m_name = name;
            return *this;
        }

        BenchmarkResults& precision(size_t prec){
            m_precision = prec;
            return *this;
        }

        template<class Str> inline void write_to_log(Str & s){
            s.append(str());
        }
    };

    // 添加对alogger的流式输出
    namespace ext{
        template<bool copy = true> inline auto to_string(const SingleBenchmarkResult & r){
            return r.str();
        }

        template<bool copy = true> inline auto to_string(const BenchmarkResults & r){
            return r.str();
        }
    }
    // 对ostream的流式输出
    inline std::ostream& operator<<(std::ostream & os,const SingleBenchmarkResult & val){
        os << val.str();
        return os;
    }
    inline std::ostream& operator<<(std::ostream & os,const BenchmarkResults val){
        os << val.str();
        return os;
    }

    /// 简易的benchmark函数
    template<IsValidBenchmarkFunction Function> struct Benchmark{
        Function func;

        // 初始化benchmark函数
        Benchmark(Function f):func(std::move(f)){}

        #pragma GCC push_options
        #pragma GCC optimize("O0")
        /**
         * @brief 运行benchmark
         * 
         * @param times 单次运行时长
         * @param repeat 重复次数
         * @return BenchmarkResults 
         * @start-date 2025/11/08
         */
        inline BenchmarkResults run(uint32_t times,uint32_t repeat){
            BenchmarkResults rs;

            /// 预热，同时计算出对volatile的修改
            {
                Clock clk;
                timespec st;
                clk.start();
                uint64_t warmTimes = 0;
                uint64_t sink = 0;
                clock_gettime(ALIB_PERF_CLOCK_SOURCE,&st);
                while(clk.get_all() < warm_up_time_ms){
                    for(int i = 0; i < warm_up_per_run_times; ++i)sink += 1;
                    warmTimes += warm_up_per_run_times;
                    sink = 0;
                }
            }

            for(uint32_t i = 0;i < repeat;++i){
                rs.results.push_back(single_run(times));
            }

            return rs;
        }
        #pragma GCC pop_options
    private:
        #pragma GCC push_options
        #pragma GCC optimize("O0")
        /// 单次运行测试
        inline SingleBenchmarkResult single_run(uint32_t times){
            SingleBenchmarkResult result = {0};
            timespec st,ed;
            clock_gettime(ALIB_PERF_CLOCK_SOURCE,&st);
            for(uint32_t i = 0;i < times;++i){
                func();
            }
            clock_gettime(ALIB_PERF_CLOCK_SOURCE,&ed);
            result.timeSum = (ed.tv_sec - st.tv_sec) * 1000 + (ed.tv_nsec - st.tv_nsec) / 1'000'000.0;
            result.times = times;
            return result;
        }
        #pragma GCC pop_options
    };
}



#endif