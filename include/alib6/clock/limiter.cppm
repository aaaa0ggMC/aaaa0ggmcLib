/**
 * @file limiter.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 自适应帧率限制器与定时器 (alib6.clock:limiter)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <chrono>
#include <thread>

export module alib6.clock:limiter;

import alib6.co;
import :core;

export namespace alib6 {

    /**
     * @brief 自适应帧率限制器 (FPS 控制器)
     */
    struct RateLimiter {
        Clock clk;
        Trigger trig;
        double desire_fps{60.0};

        explicit RateLimiter(double fps)
            : clk(true), trig(clk, 1000.0 / fps), desire_fps(fps) {}

        void wait() {
            if (desire_fps <= 0) return;
            
            while (!trig.test()) {
                double remaining = trig.duration - (clk.get_all() - trig.m_recorded_time);
                if (remaining > 1.0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(remaining * 500)));
                }
            }
        }

        [[nodiscard]] bool until() {
            return trig.test();
        }

        template<class T>
        void wait(T&& fn) {
            if (desire_fps <= 0) return;
            
            while (!trig.test()) {
                if constexpr (requires { fn(0.1); }) {
                    double remaining = trig.duration - (clk.get_all() - trig.m_recorded_time);
                    fn(remaining);
                } else {
                    fn();
                }
            }
        }

        template<class T>
        void wait(co::Task<T>& task) {
            if (desire_fps <= 0) return;
            
            while (!trig.test()) {
                if (!task.should_next()) [[unlikely]] {
                    wait();
                    return;
                } else {
                    task.next();
                }
            }
        }

        void reset(double fps) {
            desire_fps = fps;
            trig.duration = 1000.0 / fps;
            clk.reset();
            trig.reset();
        }
    };

    /**
     * @brief 高精度定时器
     */
    struct Timer {
    private:
        std::chrono::steady_clock::time_point time_point;
        std::chrono::nanoseconds time{0};
        bool waiting{false};

    public:
        explicit Timer(std::chrono::nanoseconds nanos = std::chrono::nanoseconds(0))
            : time(nanos) {}

        explicit Timer(double milliseconds)
            : time(std::chrono::nanoseconds(static_cast<std::size_t>(milliseconds * 1'000'000))) {}

        void set(std::chrono::nanoseconds nanos) {
            time = nanos;
        }

        void set(double milli) {
            time = std::chrono::nanoseconds(static_cast<std::size_t>(milli * 1'000'000));
        }

        [[nodiscard]] auto get() const -> std::chrono::nanoseconds {
            return time;
        }

        bool trigger(bool resetIfTriggered = true) {
            if (!waiting) {
                waiting = true;
                time_point = std::chrono::steady_clock::now();
            }
            auto diff = std::chrono::steady_clock::now() - time_point;
            if (diff > time) {
                if (resetIfTriggered) waiting = false;
                return true;
            }
            return false;
        }

        [[nodiscard]] bool until() {
            return trigger(true);
        }

        void wait() {
            if (waiting) std::this_thread::sleep_until(time_point + time);
            else std::this_thread::sleep_for(time);
            waiting = false;
        }

        template<class T>
        void wait(T&& fn) {
            while (!trigger()) {
                fn();
            }
        }

        template<class T>
        void wait(co::Task<T>& task) {
            while (!trigger()) {
                if (!task.should_next()) [[unlikely]] {
                    wait();
                    return;
                } else {
                    task.next();
                }
            }
        }
    };

} // namespace alib6
