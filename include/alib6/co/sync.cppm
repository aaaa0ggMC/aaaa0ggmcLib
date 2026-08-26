/**
 * @file sync.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 协程同步原语 (Signal, WaitGroup, RepetitiveWork, ThreadingWork, Race) (alib6.co:sync)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <atomic>
#include <thread>
#include <memory>
#include <future>
#include <tuple>
#include <utility>

export module alib6.co:sync;

import :concepts;

export namespace alib6::co {

    /**
     * @brief 线程安全事件信号
     */
    struct Signal {
    private:
        std::atomic<bool> m_state{false};

    public:
        Signal() = default;
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;

        void fire() noexcept {
            m_state.store(true, std::memory_order_release);
        }

        [[nodiscard]] bool ready() const noexcept {
            return m_state.load(std::memory_order_acquire);
        }

        explicit operator bool() const noexcept {
            return ready();
        }

        [[nodiscard]] bool until() const noexcept {
            return ready();
        }
    };

    /**
     * @brief 轻量级原子等待组
     */
    class WaitGroup {
        std::atomic<int> m_count{0};

        void add(int n) noexcept { m_count.fetch_add(n, std::memory_order_relaxed); }
        void done() noexcept { m_count.fetch_sub(1, std::memory_order_acq_rel); }

    public:
        class Guard;
        friend class Guard;

        struct Guard {
        private:
            WaitGroup* m_wg;
        public:
            explicit Guard(WaitGroup& wg) : m_wg(&wg) { m_wg->add(1); }
            ~Guard() { if (m_wg) m_wg->done(); }

            Guard(Guard&& other) noexcept : m_wg(other.m_wg) { other.m_wg = nullptr; }
            Guard& operator=(Guard&&) = delete;
            Guard(const Guard&) = delete;
        };

        WaitGroup() = default;

        [[nodiscard]] Guard make_guard() { return Guard(*this); }

        [[nodiscard]] bool ready() const noexcept { return m_count.load(std::memory_order_acquire) <= 0; }
        [[nodiscard]] bool until() const noexcept { return ready(); }

        bool reset() noexcept {
            if (!ready()) return false;
            m_count.store(0, std::memory_order_release);
            return true;
        }
    };

    /**
     * @brief 基于 std::jthread 的重复执行工作线程
     */
    struct RepetitiveWork {
        std::shared_ptr<std::atomic<bool>> done;
        std::jthread th;

        template<class Fn>
        RepetitiveWork(Fn&& f)
            : done(std::make_shared<std::atomic<bool>>(false)) {
            th = std::jthread([fn = std::forward<Fn>(f), d = done](std::stop_token st) {
                while (!st.stop_requested()) {
                    fn();
                }
                d->store(true, std::memory_order_release);
            });
        }

        [[nodiscard]] bool until() const {
            return done->load(std::memory_order_acquire);
        }

        void cancel() {
            th.request_stop();
        }

        void wait() {
            done->wait(false);
        }
    };

    /**
     * @brief 基于 std::future<void> 的异步工作
     */
    struct ThreadingWork {
        std::future<void> fut;

        template<class Fn>
        ThreadingWork(Fn&& f) {
            fut = std::async(std::launch::async, std::forward<Fn>(f));
        }

        [[nodiscard]] bool until() const {
            if (!fut.valid()) return true;
            return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

        void wait() {
            if (fut.valid()) fut.wait();
        }
    };

    /**
     * @brief 多条件竞态判定器
     */
    template<class... Vs>
    struct Race {
        std::tuple<Vs...> values;
        unsigned int winner{0};

        Race(Vs&&... races)
            : values(std::forward<Vs>(races)...) {}

        bool until() {
            int i = 0;
            return std::apply([&](auto&... v) {
                return ((v.until() ? (winner = i, true) : (++i, false)) || ...);
            }, values);
        }
    };

    template<class... Ts>
    Race(Ts&&...) -> Race<Ts...>;

} // namespace alib6::co
