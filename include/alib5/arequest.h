/**
 * @file arequest.h
 * @brief Producer/consumer request queue with batch stealing and a CV-driven wait policy. / 生产者/消费者请求队列，支持批量窃取与条件变量等待。
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef ALIB5_AREQUEST_H
#define ALIB5_AREQUEST_H
#include <condition_variable>
#include <mutex>
#include <alib5/autil.h>
#include <atomic>
#include <list>

namespace alib5{
    /// @brief Number of pending requests that trigger one batch boundary in the producer queue. 一批的大小
    constexpr size_t conf_steal_batch_size = 32;

    /// @brief Concept requiring @p T expose handle_request(Ctx) for use as a request type in RequestManager.
    template<class T,class Ctx>
    concept IsRequestType = requires(T & t,Ctx & tx){
        t.handle_request(ctx);
    };

    /**
     * @brief Multi-producer/multi-consumer request manager with batched stealing between two queues.
     *
     * Producers push into a pending list; consumers steal whole batches into a consumer list under a
     * double lock, then dispatch each request via std::visit. Waiting is delegated to a caller-supplied
     * wait function so callers can integrate their own event loop.
     *
     * @par Original Comment:
     * (none — file carried only inline implementation comments)
     *
     * @tparam ContextType Context passed to each request's handle_request.
     * @tparam RequestTypes Variadic set of request types held in the variant queue.
     */
    template<class ContextType,IsRequestType<ContextType>... RequestTypes>
    struct ALIB5_API RequestManager{
        /// @brief Variant list holding either monostate or any registered request type. 队列类型
        using queue_t = std::pmr::list<
            std::variant<std::monostate,RequestTypes...>
        >;
        /// @brief Deque of batch end iterators marking steal boundaries. 批次位置类型
        using batch_pos_t = std::pmr::deque<
            typename queue_t::iterator
        >;
        /// @brief Mutex guarding consumer-side queue access. Consumer之间进行竞争偷取
        std::mutex consumer_mutex;
        /// @brief Mutex paired with cv for wake-up synchronization. wake_up锁
        std::mutex wake_up_mutex;
        /// @brief Consumer-side queue after batch stealing. 消费者队列
        queue_t request_list_consumer;
        /// @brief Batch boundaries on the consumer side. 消费者批次
        batch_pos_t consumer_batches;
        /// @brief Atomic flag: true when the consumer queue is empty. 消费者是否空
        std::atomic<bool> consumer_empty { true };
        /// @brief Mutex guarding producer-side queue access. Producer之间添入数据
        std::mutex producer_mutex;
        /// @brief Batch boundaries on the producer side. 生产者批次
        batch_pos_t producer_batches;
        /// @brief Producer-side queue receiving push_request inserts. 生产者队列
        queue_t request_list_pending;
        /// @brief Atomic flag: true when the producer queue is empty. 生产者是否空
        std::atomic<bool> producer_empty { true };
        /// @brief Condition variable woken by push_request when data becomes available. cv
        std::condition_variable cv;

        /// @brief Default wait: block on cv until the producer queue is non-empty. 默认的wait函数
        static void _wait_for_cv(RequestManager * mgr){
            std::unique_lock<std::mutex> cv_lock(mgr->wake_up_mutex);
            mgr->cv.wait(cv_lock,[&mgr]{ return mgr->until(); });
        }

        /// @brief Predicate used by the wait: true when producer queue has data. 判断当前是否可以退出wait
        bool until(){
            return !producer_empty.load();
        }

        /// @brief Consume requests forever: wait, steal a batch, then dispatch each via std::visit. 消费者：消费请求
        ///
        /// @par Original Comment:
        /// (inline step comments retained below)
        ///
        /// @param context Context forwarded to each request's handle_request.
        /// @param fn Wait strategy; defaults to _wait_for_cv.
        template<class WaitFn = decltype(_wait_for_cv)>
        requires requires(WaitFn && fn,RequestManager * rq){
            fn(rq);
        }
        void handle_request(ContextType context,WaitFn && fn = _wait_for_cv){

            queue_t local_queue;
            while(true){
                // step1: 如果consumer空了尝试读取batch,如果batch还是空的,那么进行等待
                if(consumer_empty.load()){
                    // 等待producer empty为false
                    if(producer_empty.load()){
                        fn(this);
                    }
                    // 迅速偷取
                    {
                        std::scoped_lock locks(producer_mutex,consumer_mutex);
                        if(consumer_empty.load()){
                            request_list_consumer = std::move(request_list_pending);
                            consumer_batches = std::move(producer_batches);

                            request_list_pending.clear();
                            producer_batches.clear();

                            // 无论如何迅速走开
                            consumer_empty.store(false, std::memory_order_release);
                            producer_empty.store(true, std::memory_order_release);
                        }
                    }
                }
                // step2: 从消费者队列进行偷取
                {
                    std::lock_guard<std::mutex> lock(consumer_mutex);
                    if(!request_list_consumer.empty()){
                        typename queue_t::iterator end_pos = request_list_consumer.end();
                        if(!consumer_batches.empty()){
                            end_pos = consumer_batches.front();
                            consumer_batches.pop_front();
                        }
                        local_queue.splice(
                            local_queue.end(),
                            request_list_consumer,
                            request_list_consumer.begin(),
                            end_pos
                        );
                    }else{
                        consumer_empty.store(true,std::memory_order_release);
                        continue;
                    }
                }
                // 最后就是执行任务列表,此时任务都偷取过来了,因此whatever
                while(!local_queue.empty()){
                    auto & value = local_queue.front();
                    std::visit([&context](auto & v){
                        using T = std::decay_t<decltype(v)>;
                        if constexpr(IsRequestType<T,ContextType>){
                            v.handle_request(context);
                        }
                    },value);
                    local_queue.pop_front();
                }
            }
        }

        /// @brief Producer: emplace a request, record batch boundaries, and notify waiting consumers. 生产者：添加请求
        ///
        /// @par Original Comment:
        /// (inline step comments retained below)
        ///
        /// @tparam RequestType Concrete request type to construct in the queue.
        /// @tparam Args Argument types forwarded to RequestType's constructor.
        /// @param args Constructor arguments for the request.
        template<class RequestType,class... Args>
        void push_request(Args&&... args){
            size_t sz = 0;
            {
                std::lock_guard<std::mutex> lock(producer_mutex);
                request_list_pending.emplace_back().template emplace<RequestType>(
                    std::forward<Args>(args)...
                );
                // 到这里就至少为1了
                sz = request_list_pending.size();
                if(sz % conf_steal_batch_size == 0){
                    producer_batches.emplace_back(std::prev(request_list_pending.end()));
                }
            }
            if(producer_empty.load(std::memory_order_relaxed)){
                producer_empty.store(false, std::memory_order_release);
                if(sz <= conf_steal_batch_size)cv.notify_one();
                else cv.notify_all();
            }
        }
    };
};

#endif
