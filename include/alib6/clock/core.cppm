/**
 * @file core.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 高精度分段时钟与触发器 (alib6.clock:core)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <chrono>
#include <utility>

export module alib6.clock:core;

export namespace alib6 {

    using clock_source = std::chrono::steady_clock;

    /**
     * @brief 高精度分段时钟类
     */
    struct Clock {
        enum class Status {
            Running,
            Paused,
            Stopped
        };

    private:
        Status m_status{Status::Stopped};
        std::chrono::nanoseconds m_start{0};
        std::chrono::nanoseconds m_pre{0};
        std::chrono::nanoseconds m_total_gained{0};

    public:
        explicit Clock(bool start_clock = true) {
            if (start_clock) start();
        }

        void start() {
            if (m_status == Status::Running) return;

            auto now_ns = clock_source::now().time_since_epoch();
            if (m_status == Status::Stopped) {
                m_total_gained = std::chrono::nanoseconds(0);
            }
            
            m_start = now_ns;
            m_pre = now_ns;
            m_status = Status::Running;
        }

        void pause() {
            if (m_status == Status::Running) {
                m_total_gained += (clock_source::now().time_since_epoch() - m_start);
                m_status = Status::Paused;
            }
        }

        void stop() {
            m_status = Status::Stopped;
            m_total_gained = std::chrono::nanoseconds(0);
            m_start = std::chrono::nanoseconds(0);
            m_pre = std::chrono::nanoseconds(0);
        }

        void resume() {
            start();
        }

        void reset() {
            stop();
            start();
        }

        [[nodiscard]] std::pair<double, double> now() const {
            if (m_status == Status::Stopped) return {0.0, 0.0};

            auto now_ns = clock_source::now().time_since_epoch();
            
            std::chrono::nanoseconds all = m_total_gained;
            if (m_status == Status::Running) {
                all += (now_ns - m_start);
            }

            std::chrono::nanoseconds offset = std::chrono::nanoseconds(0);
            if (m_status == Status::Running) {
                offset = now_ns - m_pre;
            }

            return std::make_pair(all.count() / 1'000'000.0, offset.count() / 1'000'000.0);
        }

        void clear_offset() {
            if (m_status == Status::Running) {
                m_pre = clock_source::now().time_since_epoch();
            }
        }

        [[nodiscard]] double get_all() const {
            return now().first;
        }

        [[nodiscard]] double get_offset() const {
            return now().second;
        }

        [[nodiscard]] Status status() const {
            return m_status;
        }
    };

    /**
     * @brief 时间间隔触发器 (满足 CanUntil 概念)
     */
    struct Trigger {
        Clock* m_clock{nullptr};
        double m_recorded_time{0.0};
        double duration{0.0};

        Trigger(Clock& clock, double ms)
            : m_clock(&clock), duration(ms) {
            m_recorded_time = m_clock->get_all();
        }

        bool test(bool reset_if_succeeds = true) {
            if (duration < 0) return true;
            if (!m_clock) return false;
            
            double now_time = m_clock->get_all();
            if (now_time - m_recorded_time >= duration) {
                if (reset_if_succeeds) m_recorded_time += duration;
                return true;
            }
            return false;
        }

        void reset() {
            if (m_clock) m_recorded_time = m_clock->get_all();
        }

        [[nodiscard]] bool until() {
            return test();
        }

        void set_clock(Clock& clock) {
            m_clock = &clock;
        }
    };

} // namespace alib6
