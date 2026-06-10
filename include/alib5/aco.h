/**
 * @file aco.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Provides simple coroutine-related support. / 提供简单的和协程相关的支持
 * @version 5.0
 * @date 2026/06/10
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/09 
 */
#ifndef ALIB5_ACO
#define ALIB5_ACO

#include <alib5/adebug.h>
#include <generator>
#include <atomic>
#include <thread>
#include <memory>
#include <future>
#include <optional>
#include <tuple>
#include <type_traits>

namespace alib5::co {

    // =========================================================================
    // Concepts
    // =========================================================================

    /**
     * @brief Concept to determine if a type supports the `until()` condition evaluation.
     * 
     * @tparam T The type to check.
     */
    template<class T>
    concept CanUntil = requires(T& t) {
        { t.until() } -> std::convertible_to<bool>;
    };

    /**
     * @brief Forward declaration of Task to use in concepts.
     */
    template<class T>
    struct Task;

    /**
     * @brief Concept to determine if a type is a valid coroutine Task.
     * 
     * @tparam T The type to check.
     */
    template<class T>
    concept IsTask = requires(T&& t) {
        Task(t);
    } || requires(std::remove_cvref_t<T>& t) {
        { t.next() } -> std::same_as<void>;
        { t.should_next() } -> std::convertible_to<bool>;
    };

    // =========================================================================
    // Core Types
    // =========================================================================

    /**
     * @brief A coroutine task wrapper based on C++23 std::generator.
     * 
     * @tparam T The type yielded by the generator.
     */
    template<class T>
    struct Task {
        using gen_t = std::generator<T>;
        using iterator = decltype(std::declval<gen_t>().begin());
        
        gen_t handle;                           ///< The underlying generator handle.
        std::optional<iterator> current;        ///< An optional iterator representing the current position.
        bool inited { false };                  ///< Flag indicating whether the generator has been initialized.

        /**
         * @brief Resets the current execution state of the task.
         */
        void reset() {
            current.reset();
            inited = false;
        }

        /**
         * @brief Move constructor for the Task. Transfers execution state safely.
         * 
         * @param other The other Task to move from.
         */
        Task(Task&& other) ALIB5_NOEXCEPT 
            : handle(std::move(other.handle)) {
            panic_if(inited, "Coroutine has inited!");
            panic_if(other.inited, "Other's coroutine has inited!");
            reset();
            inited = other.inited;
            other.inited = false;
            other.current.reset();
        }

        /**
         * @brief Constructs a task directly from an rvalue generator.
         * 
         * @param t The rvalue generator.
         */
        Task(gen_t&& t)
            : handle(std::move(t)) {}

        /**
         * @brief Constructs a task using a callable (lvalue reference) and its arguments.
         * 
         * @tparam Fn Type of the callable.
         * @tparam Args Argument types for the callable.
         */
        template<class Fn, class... Args>
        requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task> && 
                  std::is_lvalue_reference_v<Fn>)
        Task(Fn&& t, Args&&... args)
            : handle(std::forward<Fn>(t)(std::forward<Args>(args)...)) {}

        /**
         * @brief Constructs a task using a callable (rvalue) that can decay to a function pointer.
         */
        template<class Fn, class... Args>
        requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task> && 
                  !std::is_lvalue_reference_v<Fn> && 
                  requires(Fn f) { +f; })
        Task(Fn&& t, Args&&... args)
            : handle((+t)(std::forward<Args>(args)...)) {}

        /**
         * @brief Deleted constructor to prevent capturing dangling references for un-decayable rvalue closures.
         */
        template<class Fn, class... Args>
        requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task> && 
                  !std::is_lvalue_reference_v<Fn> && 
                  !requires(Fn f) { +f; })
        Task(Fn&& t, Args&&... args) = delete("This may cause coroutine break!");

        /**
         * @brief Checks if the generator has further elements to yield.
         * 
         * @return true If the generator is uninitialized or has not reached the end.
         * @return false If the generator has been exhausted.
         */
        bool should_next() {
            if(!inited) return true;
            return *current != handle.end();
        }

        /**
         * @brief Resumes the coroutine to advance to the next yield point.
         */
        void next() {
            if (!inited) {
                inited = true;
                current.emplace(std::move(handle.begin()));
            } else [[likely]] {
                panic_debug(!should_next(), "Generator has terminated!");
                ++(*current);
            }
        }
    };
    
    /**
     * @par Deduction Guides for Generator / 对于generator的推导指南
     */
    template<class Fn, class... Args>
    Task(Fn&&, Args&&...) -> Task<std::ranges::range_value_t<std::remove_cvref_t<decltype(std::declval<Fn>()(std::declval<Args>()...))>>>;

    template<class T>
    Task(std::generator<T>&&) -> Task<T>;

    template<class T>
    Task(std::generator<T>&) -> Task<T>;

    /**
     * @brief A structure that specifies a fallback task when executing combined tasks.
     * 
     * @tparam U The inner task type.
     */
    template<class U>
    struct OnErr {
        U task;

        /**
         * @brief Constructs an OnErr wrapper handling a fallback task.
         * 
         * @tparam T Type that satisfies the Task requirement.
         * @param v The fallback logic.
         */
        template<class T>
        OnErr(T&& v) : task(Task(std::forward<T>(v))) {}
    };

    /**
     * @brief Deduction guide for OnErr wrapper.
     */
    template<class T>
    OnErr(T&& v) -> OnErr<decltype(Task(std::forward<T>(v)))>;

    // =========================================================================
    // Waitable Primitives (Classes modeling the CanUntil concept)
    // =========================================================================

    /**
     * @brief Thread-safe atomic signal used for event notifications.
     */
    struct Signal {
    private:
        std::atomic<bool> m_state {false};

    public:
        Signal() = default;
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;

        /**
         * @brief Fires the signal, waking up or notifying waiters.
         */
        void fire() ALIB5_NOEXCEPT {
            m_state.store(true, std::memory_order_release);
        }

        /**
         * @brief Checks if the signal has been fired.
         * 
         * @return true If fired.
         * @return false Otherwise.
         */
        bool ready() const ALIB5_NOEXCEPT {
            return m_state.load(std::memory_order_acquire);
        }

        explicit operator bool() const ALIB5_NOEXCEPT {
            return ready();
        }

        /**
         * @brief Evaluates the condition for the CanUntil concept.
         * 
         * @return true If the signal is ready.
         */
        bool until() const ALIB5_NOEXCEPT {
            return ready();
        }
    };

    /**
     * @brief Synchronization primitive that waits for a collection of operations to complete.
     * 
     * @par Recommended usage / 建议使用案例:
     * @code
     * WaitGroup wg;
     * Task task(others);
     * 
     * std::cout << "Doing main" << std::endl;
     * std::jthread th([guard = wg.make_guard()]{
     *     std::this_thread::sleep_for(std::chrono::milliseconds(110));
     * });
     * 
     * wait_until(task, wg);
     * std::cout << "Main ended" << std::endl;
     * @endcode
     * 
     * @par Pitfall / 避坑指南:
     * If you capture `wg` by reference and create the guard inside the thread like this: 
     * / 如果是如下这种在线程内创建 guard 的做法：
     * @code
     * std::jthread th([&wg]{
     *     auto guard = wg.make_guard();
     *     std::this_thread::sleep_for(std::chrono::milliseconds(110));
     * });
     * @endcode
     * You might miss the access due to timing issues. / 可能会因为时间问题从而错过访问。
     */
    class WaitGroup {
        std::atomic<int> m_count{0};

        void add(int n) ALIB5_NOEXCEPT { m_count.fetch_add(n, std::memory_order_relaxed); }
        void done() ALIB5_NOEXCEPT { m_count.fetch_sub(1, std::memory_order_acq_rel); }

    public:
        class Guard; 
        friend class Guard;

        /**
         * @brief RAII guard to safely acquire and release a count from the WaitGroup.
         */
        struct Guard {
        private:
            WaitGroup* m_wg;
        public:
            explicit Guard(WaitGroup& wg) : m_wg(&wg) { m_wg->add(1); }
            ~Guard() { if(m_wg) m_wg->done(); }

            Guard(Guard&& other) ALIB5_NOEXCEPT : m_wg(other.m_wg) { other.m_wg = nullptr; }
            Guard& operator=(Guard&&) = delete;
            Guard(const Guard&) = delete;
        };

        WaitGroup() = default;

        /**
         * @brief Creates an RAII guard that increments the wait count upon creation.
         * 
         * @return Guard The generated scoped guard.
         */
        [[nodiscard]] Guard make_guard() { return Guard(*this); }

        /**
         * @brief Checks if all registered operations have completed.
         * 
         * @return true If the internal count is zero or less.
         */
        bool ready() const ALIB5_NOEXCEPT { return m_count.load(std::memory_order_acquire) <= 0; }

        /**
         * @brief Implements the requirement for the CanUntil concept.
         * 
         * @return true If the WaitGroup is ready.
         */
        bool until() const ALIB5_NOEXCEPT { return ready(); }

        /**
         * @brief Resets the WaitGroup state if it has already been signaled.
         * 
         * @return true If reset was successful.
         * @return false If the WaitGroup is still pending.
         */
        bool reset() ALIB5_NOEXCEPT {
            if(!ready()) return false;
            m_count.store(0, std::memory_order_release); 
            return true;
        }
    };

    /**
     * @brief A threaded worker wrapper that performs a repetitive action until interrupted.
     */
    struct RepetitiveWork {
        std::shared_ptr<std::atomic<bool>> done;
        std::jthread th;

        /**
         * @brief Constructs repetitive work running inside a jthread.
         * 
         * @tparam Fn The callable type.
         * @param f The functional object defining the repetitive workload.
         */
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

        /**
         * @brief Evaluates whether the underlying workload has finished.
         */
        bool until() const {
            return done->load(std::memory_order_acquire);
        }

        /**
         * @brief Requests an explicit stop for the threaded execution.
         */
        void cancel() {
            th.request_stop(); 
        }

        /**
         * @brief Blocks until the thread fully concludes.
         */
        void wait() {
            done->wait(false);
        }
    };

    /**
     * @brief Represents an asynchronous task scheduled on another thread utilizing a future.
     */
    struct ThreadingWork {
        std::future<void> fut;

        /**
         * @brief Spawns threading work via std::async.
         * 
         * @tparam Fn Function signature.
         * @param f Function to execute asynchronously.
         */
        template<class Fn>
        ThreadingWork(Fn&& f) {
            fut = std::async(std::launch::async, std::forward<Fn>(f));
        }

        /**
         * @brief Checks if the future has safely finished execution.
         */
        bool until() const {
            if (!fut.valid()) return true;
            return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

        /**
         * @brief Blocks to wait for the future's completion.
         */
        void wait() {
            if (fut.valid()) fut.wait();
        }
    };

    /**
     * @brief Waits collectively across multiple `CanUntil` instances until the first one is resolved.
     * 
     * @tparam Vs The condition types participating in the race.
     */
    template<class... Vs>
    struct Race {
        std::tuple<Vs...> values;               ///< Tuple to store racers.
        unsigned int winner;                    ///< The index of the racer that resolves first.

        /**
         * @brief Initialize racers.
         */
        Race(Vs&&... races)
            : values(std::forward<Vs>(races)...) {}        
    
        /**
         * @brief Continuously checks if any element in the tuple evaluates to true.
         * 
         * @return true When the first racer signals completion.
         */
        bool until() {
            int i = 0;
            return std::apply([&](auto&... v){
                return ((v.until() ? (winner = i, true) : (++i, false)) || ...);
            }, values);
        }
    };

    /**
     * @brief Deduction guide for the Race structure.
     */
    template<class... Ts>
    Race(Ts&&...) -> Race<Ts...>;


    // =========================================================================
    // Algorithms
    // =========================================================================

    /**
     * @brief Steps a task explicitly until a specific condition resolves.
     * 
     * @tparam T The task generator value type.
     * @tparam Until Condition type satisfying the CanUntil concept.
     * @param t The task to advance.
     * @param ut The condition block driving the wait.
     */
    template<class T, CanUntil Until> 
    void wait_until(Task<T>& t, Until& ut) {
        while (!ut.until()) {
            panic_if(!t.should_next(), "Task expired!");
            t.next();
        }
    }

    /**
     * @brief Advances a task until *any* of the conditions resolves to true.
     * 
     * @tparam T The task generator value type.
     * @tparam Ts Types evaluating to a CanUntil condition.
     * @param task The base task to execute.
     * @param ts Multi-conditional arguments participating in the race.
     * @return size_t Index referencing the condition that won the race.
     */
    template<class T, CanUntil... Ts>
    size_t any(Task<T>& task, Ts&&... ts) {
        Race race(std::forward<Ts>(ts)...);
        while (true) {
            if (race.until()) break;
            if (task.should_next()) {
                task.next();
            } else {
                panic_if(!task.should_next(), "Task expired!");
            }
        }
        return race.winner;
    }

    /**
     * @brief Combines several tasks into a single aggregate Task that steps all sub-tasks uniformly.
     * 
     * @tparam Ts Task types to be combined.
     * @param itasks Variadic arguments comprising individual task objects.
     * @return auto A synthesized Task that drives all sub-tasks concurrently.
     */
    template<IsTask... Ts> 
    auto combine_tasks(Ts&&... itasks) {
        auto nice_forward = []<class T>(T&& t) -> auto {
            return Task(std::forward<T>(t));
        };
        
        return Task([](auto tup) mutable -> std::generator<int> {
            bool failed = true;
            while (true) {
                failed = true;
                std::apply([&failed](auto&... sub_tasks) {
                    auto execute = [&failed](auto& st) {
                        if (st.should_next()) {
                            st.next();
                            failed = false;
                        }
                    };
                    (execute(sub_tasks), ...);
                }, tup);
                
                panic_if(failed, "All of the tasks are expired!");
                co_yield 0;
            }
        }, std::make_tuple(nice_forward(std::forward<Ts>(itasks))...));
    }

    /**
     * @brief Combines multiple tasks into a single task but delegates back to a fallback task if primary tasks fail/expire.
     * 
     * @tparam L Task type for the fallback process.
     * @tparam Ts The primary task types to be combined.
     * @param fallback Formats the fallback task configured within `OnErr`.
     * @param itasks Variadic arguments mapping the initial workload.
     * @return auto A synthesized Task controlling execution and mapping errors correctly.
     */
    template<IsTask L, IsTask... Ts> 
    auto combine_tasks(OnErr<L> fallback, Ts&&... itasks) {
        auto nice_forward = []<class T>(T&& t) -> auto {
            return Task(std::forward<T>(t));
        };
        
        return Task([fb = std::move(fallback.task)](auto tup) mutable -> std::generator<int> {
            bool failed = true;
            while (true) {
                failed = true;
                std::apply([&failed](auto&... sub_tasks) {
                    auto execute = [&failed](auto& st) {
                        if (st.should_next()) {
                            st.next();
                            failed = false;
                        }
                    };
                    (execute(sub_tasks), ...);
                }, tup);
                
                if (failed) fb.next();
                co_yield 0;
            }
        }, std::make_tuple(nice_forward(std::forward<Ts>(itasks))...));
    }

} // namespace alib5::co

#endif // ALIB5_ACO