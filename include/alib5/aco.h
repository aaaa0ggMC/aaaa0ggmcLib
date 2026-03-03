/**
 * @file aco.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 提供简单的和协程相关的支持
 * @version 5.0
 * @date 2026/03/03
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

        void reset(){
            current.reset();
            inited = false;
        }

        Task(Task&& other) noexcept 
        :handle(std::move(other.handle)){
            panic_if(inited, "Coroutine has inited!");
            panic_if(other.inited, "Other's coroutine has inited!");
            reset();
            inited = other.inited;
            other.inited = false;
            other.current.reset();
        }

        Task(gen_t && t)
        :handle(std::move(t)){}

        template<class Fn,class... Args> Task(Fn && t,Args&&... args)
        requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task>)
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
    template<class T>
    Task(std::generator<T>&&) -> Task<T>;
    template<class T>
    Task(std::generator<T>&) -> Task<T>;


    template<class T>
    concept IsTask = requires(T && t){
        Task(t);
    } || requires(std::remove_cvref_t<T>& t) {
        { t.next() } -> std::same_as<void>;
        { t.should_next() } -> std::convertible_to<bool>;
    };

    struct Signal{
    private:
        std::atomic<bool> m_state {false};
        friend struct CancelableWaitGroup;
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

    template<IsTask... Ts> auto combine_tasks(Ts&&... itasks){
        auto nice_forward = []<class T>(T && t) -> auto {
            return Task(std::forward<T>(t));
        };
        return Task([](auto tup) mutable -> std::generator<int>{
            bool failed = true;
            while(true){
                failed = true;
                std::apply([&failed](auto&... sub_tasks){
                    auto execute = [&failed](auto& st){
                        if(st.should_next()) {
                            st.next();
                            failed = false;
                        }
                    };
                    (execute(sub_tasks), ...);
                }, tup);
                panic_if(failed,"All of the tasks are expired!");
                co_yield 0;
            }
        },std::make_tuple(nice_forward(std::forward<Ts>(itasks))...));
    }

    struct CancelableWaitGroup {
        Signal signal;
        WaitGroup wg;

        void cancel(){
            signal.fire();
        }
        bool should_cancel() const { return signal.ready(); }

        bool until(){ return wg.until(); }
        bool ready(){ return wg.ready(); }
        bool reset(){
            panic_if(!wg.ready(),"There's task still running!");
            signal.m_state.store(false);
            return wg.reset();
        }
        auto make_guard(){
            return wg.make_guard();
        }
    };

    template<class T,CanUntil Until> void wait_until(Task<T> & t,Until & ut){
        while(!ut.until()){
            panic_if(!t.should_next(), "Task expired!");
            t.next();
        }
    }

    struct RepetitiveWork{
        std::shared_ptr<std::atomic<bool>> done;

        template<class Fn>
        RepetitiveWork(Fn && f,CancelableWaitGroup & cwg)
        :done(std::make_shared<std::atomic<bool>>()){
            done->store(false);
            std::thread(
            [guard = cwg.make_guard(),fn = std::forward<Fn>(f),&cwg, finished = done]
            {
                while(!cwg.should_cancel()){
                    fn();
                }
                finished->store(true);
                finished->notify_all();
            }).detach();
        }

        bool until(){
            return done->load();
        }

        void wait(){
            done->wait(false);
        }
    };

    struct ThreadingWork{
        std::shared_ptr<std::atomic<bool>> done;

        template<class Fn>
        ThreadingWork(Fn && f)
        :done(std::make_shared<std::atomic<bool>>()){
            done->store(false);
            std::thread([fn = std::forward<Fn>(f), finished = done] {
                fn();
                finished->store(true);
                finished->notify_all();
            }).detach();
        }

        bool until(){
            return done->load();
        }

        void wait(){
            done->wait(false);
        }
    };

    template<class... Vs>
    struct Race{
        std::tuple<Vs...> values;
        unsigned int winner;

        Race(Vs&&... races)
        :values(std::forward<Vs>(races)...){}        
    
        bool until(){
            int i = 0;
            return std::apply([&](auto&... v){
                return ((v.until() ? (winner = i, true) : (++i, false)) || ...);
            }, values);
        }
    };

    template<class... Ts>
    Race(Ts&&...) -> Race<Ts...>;

    template<class T,CanUntil... Ts>
    size_t any(Task<T> & task,Ts&&... ts){
        Race race(std::forward<Ts>(ts)...);
        while(true){
            if(race.until())break;
            if(task.should_next()){
                task.next();
            }else{
                panic_if(!task.should_next(),"Task expired!");
            }
        }
        return race.winner;
    }
}


#endif