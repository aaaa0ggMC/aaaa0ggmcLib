/**@file autil.h
* @brief 时钟库，提供实用计时类包装
* @author aaaa0ggmc
* @date 2026/01/28
* @version 5.0
* @copyright Copyright(c) 2026
*/
#ifndef ALIB5_ACLOCK
#define ALIB5_ACLOCK
#include <alib5/autil.h>
#include <chrono>
#include <thread>

namespace alib5{
    /// 时钟源
    using clock_source = std::chrono::steady_clock;

    /// 时钟类
    struct ALIB5_API Clock{
        /// 运行状态
        enum ClockStatus{
            Running, ///< 正在运行
            Paused,  ///< 暂停中
            Stopped  ///< 停止了
        };
    private:
        /// 当前状态
        ClockStatus m_status = Stopped;
        /// 本次运行段的起点
        std::chrono::nanoseconds m_start{0};
        /// 上一次 clear_offset 的点
        std::chrono::nanoseconds m_pre{0};
        /// 历史累积运行的总时长（不含当前这一段）
        std::chrono::nanoseconds m_total_gained{0};

    public:
        /// @param start_clock 是否启动时钟，默认启动
        inline Clock(bool start_clock = true){
            if(start_clock)start();
        }

        /// 启动或恢复时钟
        inline void start(){
            if(m_status == Running)return;

            auto now_ns = clock_source::now().time_since_epoch();
            if(m_status == Stopped){
                m_total_gained = std::chrono::nanoseconds(0);
            }
            // 无论是从Stopped还是Paused进入，都开启一个新的“计费段”
            m_start = now_ns;
            m_pre = now_ns;
            m_status = Running;
        }

        /// 暂停时钟：结算当前段的时间进入累积池
        inline void pause(){
            if(m_status == Running){
                m_total_gained += (clock_source::now().time_since_epoch() - m_start);
                m_status = Paused;
            }
        }

        /// 停止时钟：彻底重置
        inline void stop(){
            m_status = Stopped;
            m_total_gained = std::chrono::nanoseconds(0);
            m_start = std::chrono::nanoseconds(0);
            m_pre = std::chrono::nanoseconds(0);
        }

        /// 恢复时钟
        inline void resume(){
            start();
        }

        /// 重置时钟
        inline void reset(){
            stop();
            start();
        }

        /// 获取当前时间信息 {all, offset}
        inline std::pair<double,double> now(){
            if(m_status == Stopped)return {0.0,0.0};

            auto now_ns = clock_source::now().time_since_epoch();
            
            // 计算 AllTime：累积池 + (如果是运行中 ? 当前段时长 : 0)
            std::chrono::nanoseconds all = m_total_gained;
            if(m_status == Running){
                all += (now_ns - m_start);
            }

            // 计算 Offset：如果是运行中 ? 距离 pre 的时长 : 0
            std::chrono::nanoseconds offset = std::chrono::nanoseconds(0);
            if(m_status == Running){
                offset = now_ns - m_pre;
            }

            return std::make_pair(all.count()/1'000'000.0,offset.count()/1'000'000.0);
        }

        /// 清除offset
        inline void clear_offset(){
            if(m_status == Running){
                m_pre = clock_source::now().time_since_epoch();
            }
        }

        /// 获取总有效时长
        inline double get_all(){
            return now().first;
        }

        /// 获取偏移时长
        inline double get_offset(){
            return now().second;
        }

        /// 返回目前的状态
        inline ClockStatus status() const{return m_status;}
    };

    /// 触发器：用于判断是否到达特定时间间隔
    struct ALIB5_API Trigger{
        Clock* m_clock;
        double m_recorded_time;
        double duration;

        inline Trigger(Clock& clock,double ms):m_clock(&clock),duration(ms){
            m_recorded_time = m_clock->get_all();
        }

        /// 测试是否触发
        /// @param reset_if_succeeds 触发后是否自动重置基准时间
        inline bool test(bool reset_if_succeeds = true){
            if(!m_clock)return false;
            double now_time = m_clock->get_all();
            if(now_time - m_recorded_time >= duration){
                if(reset_if_succeeds)m_recorded_time += duration;
                return true;
            }
            return false;
        }

        /// 手动重置触发基准
        inline void reset(){
            if(m_clock)m_recorded_time = m_clock->get_all();
        }

        inline void set_clock(Clock& clock){m_clock = &clock;}
    };

    /// 速率限制器：常用于控制帧率 (FPS)
    struct ALIB5_API RateLimiter{
        Clock clk;
        Trigger trig;
        double desire_fps;

        inline RateLimiter(double fps):clk(true),trig(clk,1000.0/fps),desire_fps(fps){}

        /// 等待直到下一帧时间到达
        inline void wait(){
            // 如果没到时间，则进行补偿性睡眠
            while(!trig.test()){
                double remaining = trig.duration - (clk.get_all() - trig.m_recorded_time);
                if(remaining > 1.0){
                    // 只有剩余时间较长时才真正 sleep，避免调度开销导致不准
                    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(remaining * 500)));
                }
            }
        }

        /// 重新设置 FPS
        inline void reset(double fps){
            desire_fps = fps;
            trig.duration = 1000.0 / fps;
            clk.reset();
            trig.reset();
        }
    };
}

#endif