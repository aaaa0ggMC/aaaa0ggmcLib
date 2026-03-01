/**
 * @file aco.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 提供简单的和协程相关的支持
 * @version 5.0
 * @date 2026/03/01
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

namespace alib5::co{
    template<class T>
    concept CanUntil = requires(T & t){
        {t.until()} -> std::convertible_to<bool>;
    };
    
    template<class T>
    struct Task{
        using gen_t = std::generator<T>;
        using iterator = decltype(std::declval<gen_t>().begin());
        gen_t handle;
        std::optional<iterator> current;
        bool inited { false };

        template<class Fn,class... Args> Task(Fn && t,Args&&... args)
        :handle(std::forward<Fn>(t)(std::forward<Args>(args)...)){}

        bool should_next(){
            if(!inited)return true;
            return *current != handle.end();
        }

        void next(){
            if(!inited){
                inited = true;
                current.emplace(std::move(handle.begin()));
            }else [[likely]] {
                panic_debug(!should_next(), "Generator has terminated!");
                ++(*current);
            }
        }
    };
    
    // 对于generator
    template<class Fn, class... Args>
    Task(Fn&&, Args&&...) -> Task<std::ranges::range_value_t<std::remove_cvref_t<decltype(std::declval<Fn>()(std::declval<Args>()...))>>>;

    struct Signal{
    private:
        std::atomic<bool> m_state {false};
    public:
        Signal() = default;
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;

        void fire() noexcept {
            m_state.store(true, std::memory_order_release);
        }

        bool ready() const noexcept {
            return m_state.load(std::memory_order_acquire);
        }

        explicit operator bool() const noexcept {
            return ready();
        }

        bool until() const noexcept {
            return ready();
        }
    };

    /**
        @par 建议使用案例
        @code
        WaitGroup wg;
        Task task(others);

        std::cout << "Doing main" << std::endl;
        std::jthread th([guard = wg.make_guard()]{
            std::this_thread::sleep_for(std::chrono::milliseconds(110));
        });

        wait_until(task, wg);
        std::cout << "Main ended" << std::endl;
        @endcode
        如果是
        @code
        std::jthread th([&wg]{
            auto guard = wg.make_guard();
            std::this_thread::sleep_for(std::chrono::milliseconds(110));
        });
        可能会因为时间问题从错过访问
        @endcode
    */
    class WaitGroup{
        std::atomic<int> m_count{0};

        void add(int n) noexcept { m_count.fetch_add(n, std::memory_order_relaxed); }
        void done() noexcept { m_count.fetch_sub(1, std::memory_order_acq_rel); }

    public:
        class Guard; 
        friend class Guard;

        struct Guard{
        private:
            WaitGroup* m_wg;
        public:
            explicit Guard(WaitGroup& wg) : m_wg(&wg){ m_wg->add(1); }
            ~Guard(){ if(m_wg)m_wg->done(); }

            Guard(Guard&& other) noexcept : m_wg(other.m_wg){ other.m_wg = nullptr; }
            Guard& operator=(Guard&&) = delete;
            Guard(const Guard&) = delete;
        };
        WaitGroup() = default;

        [[nodiscard]] Guard make_guard(){ return Guard(*this); }

        bool ready() const noexcept { return m_count.load(std::memory_order_acquire) <= 0; }
        bool until() const noexcept { return ready(); }

        bool reset() noexcept {
            if(!ready())return false;
            m_count.store(0, std::memory_order_release); 
            return true;
        }
    };

    template<class T,CanUntil Until> void wait_until(Task<T> & t,Until & ut){
        while(!ut.until()){
            panic_if(!t.should_next(), "Task expired!");
            t.next();
        }
    }
}


#endif