/**
 * @file aperf.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一个简单的性能计算库，能确保数据大致准确，同时省的我每次都要写差不多的benchmark代码
 * @version 5.0
 * @date 2026/04/07
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
#include <vector>
#include <type_traits>
#include <limits>

namespace alib5{

    /**
     * @brief 阻止编译器优化掉某个变量。使用GCC/Clang的内联汇编实现，确保在GCC下绝对可用。
     * 
     * @tparam T 变量类型
     * @param value 变量引用
     */
    template <typename T>
    inline __attribute__((always_inline)) void do_not_optimize(T const& value){
        #if defined(__GNUC__) || defined(__clang__)
            asm volatile("" : : "g"(value) : "memory");
        #else
            static_cast<void>(value);
        #endif
    }

    /**
     * @brief 阻止编译器跨越此行进行内存优化（内存屏障）。
     */
    inline __attribute__((always_inline)) void clobber_memory(){
        #if defined(__GNUC__) || defined(__clang__)
            asm volatile("" : : : "memory");
        #else
            // 非 GCC/Clang 环境下的回退方案
        #endif
    }

    /// @brief 进行热身的最低热身时长
    constexpr uint32_t warm_up_time_ms = 100;
    
    template<class T> concept IsValidBenchmarkFunction = requires(T & t){
        t();
    };

    /// 单次运行结果
    struct SingleBenchmarkResult{
        double timeSum; ///< 单次运行总时间 (ms)
        uint32_t times; ///< 运行总次数

        /// 输出为string
        inline std::string str() const{
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6);
            ss << std::setw(12) << "TimeCost" << " : " << timeSum << " ms\n"
               << std::setw(12) << "RunTimes" << " : " << times << "\n"
               << std::setw(12) << "Average"  << " : " << (times > 0 ? timeSum/times : 0) << " ms";
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
            double shortest_avg = std::numeric_limits<double>::max();
            double longest_avg = 0;
            uint32_t times = 0;
            double stddev = 0;
            double global_aver = 0;
            double cv = 0;
        };

        CalculateInfo calculate() const {
            CalculateInfo c;
            if(results.empty()) return c;

            for(auto & t : results){
                c.sum += t.timeSum;
                c.times += t.times;
            }
            if(!c.times)return c;
            c.global_aver = c.sum / c.times;
            for(auto & t : results){
                double thisAver = t.timeSum / t.times;

                if(thisAver > c.longest_avg)c.longest_avg = thisAver;
                if(thisAver < c.shortest_avg)c.shortest_avg = thisAver;

                c.stddev += (thisAver - c.global_aver)*(thisAver-c.global_aver) * t.times;
            }
            c.stddev = std::sqrt(c.stddev / (c.times > 1 ? c.times - 1 : 1));
            c.cv = (c.global_aver != 0) ? (c.stddev / c.global_aver * 100) : 0;
            return c;
        }

        /// 输出为string
        inline std::string str() const{
            if(results.empty())return "";
            auto info = calculate();

            std::stringstream ss;
            auto norm_insert = [&](double v) -> auto& {
                auto p = misc::normalize_elapse(v);
                ss << std::fixed << std::setprecision(m_precision) <<  p.first << p.second;
                return ss;
            };

            ss << "\n-----------------------" << "\n";
            ss << m_name << "\n\n";
            ss << std::left;
            ss << std::setw(16) << "TimeCost" << " : ";
            norm_insert(info.sum) << "\n"
               << std::setw(16) << "RunTimes" << " : " << info.times << "\n"
               << std::setw(16) << "Average"  << " : ";
            norm_insert(info.global_aver) << "\n"
               << std::setw(16) << "ShortestAvg" << " : ";
            norm_insert(info.shortest_avg) << "\n"
               << std::setw(16) << "LongestAvg"  << " : ";
            norm_insert(info.longest_avg) << "\n"
               << std::setw(16) << "Stddev"  << " : " << std::fixed << std::setprecision(m_precision) << info.stddev << "\n"
               << std::setw(16) << "CV"      << " : " << std::fixed << std::setprecision(4) << info.cv << "%";
            ss << "\n--------------------------------";
            return ss.str();
        }

        BenchmarkResults& name(std::string name){
            m_name = std::move(name);
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
    inline std::ostream& operator<<(std::ostream & os,const BenchmarkResults & val){
        os << val.str();
        return os;
    }

    /// 简易的benchmark类
    template<IsValidBenchmarkFunction Function> struct Benchmark{
        Function func;

        // 初始化benchmark函数
        Benchmark(Function f):func(std::move(f)){}

        /**
         * @brief 运行benchmark
         * 
         * @param times 单次运行迭代次数
         * @param repeat 重复运行组数
         * @return BenchmarkResults 
         */
        inline BenchmarkResults run(uint32_t times,uint32_t repeat){
            BenchmarkResults rs;

            /// 预热：让 CPU 进入高频状态并预热缓存
            {
                Clock clk;
                clk.start();
                while(clk.get_all() < warm_up_time_ms){
                    // 预热时执行一小部分迭代
                    for(uint32_t i = 0; i < 100; ++i){
                         if constexpr (std::is_void_v<std::invoke_result_t<Function>>) {
                            func();
                            clobber_memory();
                        } else {
                            auto r = func();
                            do_not_optimize(r);
                        }
                    }
                }
            }

            rs.results.reserve(repeat);
            for(uint32_t i = 0; i < repeat; ++i){
                rs.results.push_back(single_run(times));
            }

            return rs;
        }

    private:
        /// 单次运行测试
        inline SingleBenchmarkResult single_run(uint32_t times){
            SingleBenchmarkResult result = {0, times};
            
            auto st = std::chrono::steady_clock::now();
            for(uint32_t i = 0; i < times; ++i){
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
}

#endif
