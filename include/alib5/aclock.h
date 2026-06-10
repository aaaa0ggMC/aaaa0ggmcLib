/**
 * @file aclock.h
 * @brief Clock library providing practical timing class wrappers. / 时钟库，提供实用计时类包装
 * @author aaaa0ggmc
 * @date 2026/06/10
 * @version 5.0
 * @copyright Copyright(c) 2026
 */

#ifndef ALIB5_ACLOCK
#define ALIB5_ACLOCK

#include <alib5/autil.h>
#include <alib5/aco.h>
#include <chrono>
#include <thread>
#include <utility>

namespace alib5 {

    /**
     * @brief The steady clock used as the primary time source.
     */
    using clock_source = std::chrono::steady_clock;

    /**
     * @brief A precise clock class for advanced time tracking and segment management.
     */
    struct ALIB5_API Clock {
        /**
         * @brief Enum representing the current operational status of the clock.
         */
        enum ClockStatus {
            Running, ///< The clock is currently running.
            Paused,  ///< The clock is paused.
            Stopped  ///< The clock is stopped and reset.
        };

    private:
        ClockStatus m_status = Stopped;                             ///< Current operational status of the clock.
        std::chrono::nanoseconds m_start{0};                        ///< Start time of the current execution segment.
        std::chrono::nanoseconds m_pre{0};                          ///< Timestamp of the last clear_offset operation.
        std::chrono::nanoseconds m_total_gained{0};                 ///< Total accumulated running time (excluding the current segment).

    public:
        /**
         * @brief Constructor for the Clock class.
         * @param start_clock Whether to start the clock immediately upon creation (default is true).
         */
        explicit inline Clock(bool start_clock = true);

        /**
         * @brief Starts or resumes the clock.
         * 
         * @par Original Comment (原注释)
         * Whether entering from Stopped or Paused, it starts a new "billing segment".
         * (无论是从Stopped还是Paused进入，都开启一个新的“计费段”)
         */
        inline void start();

        /**
         * @brief Pauses the clock and commits the current segment to the total accumulated time.
         * 
         * @par Original Comment (原注释)
         * Pause clock: settles the time of the current segment into the accumulation pool.
         * (暂停时钟：结算当前段的时间进入累积池)
         */
        inline void pause();

        /**
         * @brief Stops the clock and completely resets all accumulated time and states.
         */
        inline void stop();

        /**
         * @brief Resumes the clock from a paused state (alias for start).
         */
        inline void resume();

        /**
         * @brief Resets the clock by stopping and starting it again.
         */
        inline void reset();

        /**
         * @brief Retrieves the current time information.
         * 
         * @par Original Comment (原注释)
         * Calculate AllTime: Accumulation pool + (If running ? duration of current segment : 0).
         * Calculate Offset: If running ? duration since 'pre' : 0.
         * (计算 AllTime：累积池 + (如果是运行中 ? 当前段时长 : 0)
         * 计算 Offset：如果是运行中 ? 距离 pre 的时长 : 0)
         * 
         * @return A std::pair where 'first' is the total active duration (in ms) and 'second' is the offset duration (in ms).
         */
        inline std::pair<double, double> now() const;

        /**
         * @brief Clears the current offset baseline.
         */
        inline void clear_offset();

        /**
         * @brief Retrieves the total effective running time.
         * @return The total time in milliseconds.
         */
        inline double get_all() const;

        /**
         * @brief Retrieves the current offset time.
         * @return The offset time in milliseconds.
         */
        inline double get_offset() const;

        /**
         * @brief Gets the current running status of the clock.
         * @return The ClockStatus enumeration value.
         */
        inline ClockStatus status() const;
    };

    /**
     * @brief A trigger utility used to determine if a specific time interval has elapsed.
     * 
     * @par Original Comment (原注释)
     * Trigger: Used to determine if a specific time interval has been reached.
     * (触发器：用于判断是否到达特定时间间隔)
     */
    struct ALIB5_API Trigger {
        Clock* m_clock;         ///< Pointer to the associated clock instance.
        double m_recorded_time; ///< The baseline time recorded for comparison.
        double duration;        ///< The target interval duration in milliseconds.

        /**
         * @brief Constructor for the Trigger class.
         * @param clock Reference to the Clock instance to be used.
         * @param ms The target duration interval in milliseconds.
         */
        inline Trigger(Clock& clock, double ms);

        /**
         * @brief Tests whether the specified duration has elapsed since the last reset.
         * @param reset_if_succeeds If true, automatically resets the baseline time upon a successful trigger.
         * @return True if the duration has been reached or exceeded, false otherwise.
         */
        inline bool test(bool reset_if_succeeds = true);

        /**
         * @brief Manually resets the trigger's baseline time to the current clock time.
         */
        inline void reset();

        /**
         * @brief Alias for test() with default parameters.
         * @return True if the duration has been reached.
         */
        inline bool until();

        /**
         * @brief Assigns a new clock to the trigger.
         * @param clock Reference to the new Clock instance.
         */
        inline void set_clock(Clock& clock);
    };

    /**
     * @brief A rate limiter utility typically used to control frame rate (FPS).
     * 
     * @par Original Comment (原注释)
     * Rate Limiter: commonly used to control frame rate (FPS).
     * (速率限制器：常用于控制帧率 (FPS))
     */
    struct ALIB5_API RateLimiter {
        Clock clk;          ///< Internal clock used for rate limiting.
        Trigger trig;       ///< Internal trigger used to measure frame intervals.
        double desire_fps;  ///< The desired frames per second.

        /**
         * @brief Constructor for the RateLimiter class.
         * @param fps The desired frame rate (Frames Per Second).
         */
        inline RateLimiter(double fps);

        /**
         * @brief Blocks the current thread until the next frame's time arrives.
         * 
         * @par Original Comment (原注释)
         * If the time is not up, perform compensatory sleep. Only truly sleep when the remaining time is long to avoid inaccuracy caused by scheduling overhead.
         * (如果没到时间，则进行补偿性睡眠。只有剩余时间较长时才真正 sleep，避免调度开销导致不准)
         */
        inline void wait();

        /**
         * @brief Checks if the time for the next frame has arrived without blocking.
         * @return True if the next frame time has arrived, false otherwise.
         */
        inline bool until();

        /**
         * @brief Waits until the next frame time, executing a fallback function while waiting.
         * @tparam T The callable type.
         * @param fn The fallback function to execute during idle time. Can optionally take a double parameter (remaining time).
         */
        template<class T>
        inline void wait(T&& fn);

        /**
         * @brief Waits until the next frame time, yielding a coroutine task while waiting.
         * @tparam T The return type of the coroutine task.
         * @param task The coroutine task to yield/process during the wait time.
         */
        template<class T>
        inline void wait(co::Task<T>& task);

        /**
         * @brief Resets the rate limiter with a new target FPS.
         * @param fps The new desired frames per second.
         */
        inline void reset(double fps);
    };

    /**
     * @brief A generic timer utility for tracking and blocking until specific durations elapse.
     */
    struct ALIB5_API Timer {
    private:
        std::chrono::steady_clock::time_point time_point; ///< The point in time when the timer started waiting.
        std::chrono::nanoseconds time;                    ///< The target duration to wait.
        bool waiting{ false };                            ///< Flag indicating if the timer is currently actively waiting.

    public:
        /**
         * @brief Constructs a Timer with a nanosecond duration.
         * @param nanos Target duration in nanoseconds.
         */
        explicit Timer(std::chrono::nanoseconds nanos = std::chrono::nanoseconds(0));

        /**
         * @brief Constructs a Timer with a millisecond duration.
         * @param milliseconds Target duration in milliseconds.
         */
        explicit Timer(double milliseconds);

        /**
         * @brief Updates the target duration in nanoseconds.
         * @param nanos New target duration.
         */
        void set(std::chrono::nanoseconds nanos);

        /**
         * @brief Updates the target duration in milliseconds.
         * @param milli New target duration.
         */
        void set(double milli);

        /**
         * @brief Retrieves the current target duration.
         * @return The target duration in nanoseconds.
         */
        auto get() const -> std::chrono::nanoseconds;

        /**
         * @brief Tests if the timer's duration has elapsed since it began waiting.
         * @param resetIfTriggered If true, resets the waiting flag upon a successful trigger.
         * @return True if the duration has elapsed, false otherwise.
         */
        bool trigger(bool resetIfTriggered = true);

        /**
         * @brief Alias for trigger(true).
         * @return True if the duration has elapsed.
         */
        bool until();

        /**
         * @brief Blocks the current thread until the timer's duration has fully elapsed.
         */
        void wait();

        /**
         * @brief Waits for the timer to elapse, executing a callable while waiting.
         * @tparam T The callable type.
         * @param fn The function to execute in a loop until the timer elapses.
         */
        template<class T>
        inline void wait(T&& fn);

        /**
         * @brief Waits for the timer to elapse, yielding a coroutine task while waiting.
         * 
         * @par Original Comment (原注释)
         * If the time is not up, perform compensatory sleep.
         * (如果没到时间，则进行补偿性睡眠)
         * 
         * @tparam T The return type of the coroutine task.
         * @param task The coroutine task to yield/process.
         */
        template<class T>
        inline void wait(co::Task<T>& task);
    };

    /**
     * @brief A coroutine generator that sleeps for a specified nanosecond duration at each yield.
     * @param nap_time The duration to sleep per cycle.
     * @return A generator yielding an integer sequence.
     */
    std::generator<int> inline nap(std::chrono::nanoseconds nap_time);

    /**
     * @brief A coroutine generator that sleeps for a specified millisecond duration at each yield.
     * @param millisecs The duration to sleep per cycle in milliseconds.
     * @return A generator yielding an integer sequence.
     */
    std::generator<int> inline nap(double millisecs);

    /**
     * @brief A coroutine generator that immediately yields the current thread at each cycle.
     * @return A generator yielding an integer sequence.
     */
    std::generator<int> inline nap0();

    // =========================================================================
    // Implementation Section
    // =========================================================================

    // Clock Implementation
    inline Clock::Clock(bool start_clock) {
        if (start_clock) start();
    }

    inline void Clock::start() {
        if (m_status == Running) return;

        auto now_ns = clock_source::now().time_since_epoch();
        if (m_status == Stopped) {
            m_total_gained = std::chrono::nanoseconds(0);
        }
        
        m_start = now_ns;
        m_pre = now_ns;
        m_status = Running;
    }

    inline void Clock::pause() {
        if (m_status == Running) {
            m_total_gained += (clock_source::now().time_since_epoch() - m_start);
            m_status = Paused;
        }
    }

    inline void Clock::stop() {
        m_status = Stopped;
        m_total_gained = std::chrono::nanoseconds(0);
        m_start = std::chrono::nanoseconds(0);
        m_pre = std::chrono::nanoseconds(0);
    }

    inline void Clock::resume() {
        start();
    }

    inline void Clock::reset() {
        stop();
        start();
    }

    inline std::pair<double, double> Clock::now() const {
        if (m_status == Stopped) return {0.0, 0.0};

        auto now_ns = clock_source::now().time_since_epoch();
        
        std::chrono::nanoseconds all = m_total_gained;
        if (m_status == Running) {
            all += (now_ns - m_start);
        }

        std::chrono::nanoseconds offset = std::chrono::nanoseconds(0);
        if (m_status == Running) {
            offset = now_ns - m_pre;
        }

        return std::make_pair(all.count() / 1'000'000.0, offset.count() / 1'000'000.0);
    }

    inline void Clock::clear_offset() {
        if (m_status == Running) {
            m_pre = clock_source::now().time_since_epoch();
        }
    }

    inline double Clock::get_all() const {
        return now().first;
    }

    inline double Clock::get_offset() const {
        return now().second;
    }

    inline Clock::ClockStatus Clock::status() const {
        return m_status;
    }

    // Trigger Implementation
    inline Trigger::Trigger(Clock& clock, double ms) : m_clock(&clock), duration(ms) {
        m_recorded_time = m_clock->get_all();
    }

    inline bool Trigger::test(bool reset_if_succeeds) {
        if (duration < 0) return true;
        if (!m_clock) return false;
        
        double now_time = m_clock->get_all();
        if (now_time - m_recorded_time >= duration) {
            if (reset_if_succeeds) m_recorded_time += duration;
            return true;
        }
        return false;
    }

    inline void Trigger::reset() {
        if (m_clock) m_recorded_time = m_clock->get_all();
    }

    inline bool Trigger::until() {
        return test();
    }

    inline void Trigger::set_clock(Clock& clock) {
        m_clock = &clock;
    }

    // RateLimiter Implementation
    inline RateLimiter::RateLimiter(double fps) 
        : clk(true), trig(clk, 1000.0 / fps), desire_fps(fps) {}

    inline void RateLimiter::wait() {
        if (desire_fps <= 0) return;
        
        while (!trig.test()) {
            double remaining = trig.duration - (clk.get_all() - trig.m_recorded_time);
            if (remaining > 1.0) {
                std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(remaining * 500)));
            }
        }
    }

    inline bool RateLimiter::until() {
        return trig.test();
    }

    template<class T>
    inline void RateLimiter::wait(T&& fn) {
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
    inline void RateLimiter::wait(co::Task<T>& task) {
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

    inline void RateLimiter::reset(double fps) {
        desire_fps = fps;
        trig.duration = 1000.0 / fps;
        clk.reset();
        trig.reset();
    }

    // Timer Implementation
    inline Timer::Timer(std::chrono::nanoseconds nanos)
        : time(nanos) {}

    inline Timer::Timer(double milliseconds)
        : time(std::chrono::nanoseconds(static_cast<size_t>(milliseconds * 1'000'000))) {}

    inline void Timer::set(std::chrono::nanoseconds nanos) {
        time = nanos;
    }

    inline void Timer::set(double milli) {
        time = std::chrono::nanoseconds(static_cast<size_t>(milli * 1'000'000));
    }

    inline auto Timer::get() const -> std::chrono::nanoseconds {
        return time;
    }

    inline bool Timer::trigger(bool resetIfTriggered) {
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

    inline bool Timer::until() {
        return trigger(true);
    }

    inline void Timer::wait() {
        if (waiting) std::this_thread::sleep_until(time_point + time);
        else std::this_thread::sleep_for(time);
        waiting = false;
    }

    template<class T>
    inline void Timer::wait(T&& fn) {
        while (!trigger()) {
            fn();
        }
    }

    template<class T>
    inline void Timer::wait(co::Task<T>& task) {
        while (!trigger()) {
            if (!task.should_next()) [[unlikely]] {
                wait();
                return;
            } else {
                task.next();
            }
        }
    }

    // Coroutine Generators Implementation
    std::generator<int> inline nap(std::chrono::nanoseconds nap_time) {
        while (true) {
            std::this_thread::sleep_for(nap_time);
            co_yield 0;
        }
    }

    std::generator<int> inline nap(double millisecs) {
        auto t = std::chrono::nanoseconds(static_cast<size_t>(millisecs * 1'000'000));
        while (true) {
            std::this_thread::sleep_for(t);
            co_yield 0;
        }
    }

    std::generator<int> inline nap0() {
        while (true) {
            std::this_thread::yield();
            co_yield 0;
        }
    }

} // namespace alib5

#endif // ALIB5_ACLOCK