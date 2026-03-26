#ifndef ALIB5_AREQUEST_H
#define ALIB5_AREQUEST_H
#include <condition_variable>
#include <mutex>
#include <alib5/autil.h>
#include <atomic>
#include <list>

namespace alib5{
    constexpr size_t conf_steal_batch_size = 32;

    template<class T,class Ctx>
    concept IsRequestType = requires(T & t,Ctx & tx){
        t.handle_request(tx);
    };

    template<class ContextType,IsRequestType<ContextType>... RequestTypes>
    struct ALIB5_API RequestManager{
        using queue_t = std::pmr::list<
            std::variant<std::monostate,RequestTypes...>
        >;
        using batch_pos_t = std::pmr::deque<
            typename queue_t::iterator
        >;
        // consumer之间进行竞争偷取
        std::mutex consumer_mutex;
        std::mutex wake_up_mutex;
        queue_t request_list_consumer;
        batch_pos_t consumer_batches;
        std::atomic<bool> consumer_empty { true }; 
        // producer之间添入数据
        std::mutex producer_mutex;
        batch_pos_t producer_batches;
        queue_t request_list_pending;
        std::atomic<bool> producer_empty { true }; 
        std::condition_variable cv;

        static void _wait_for_cv(RequestManager * mgr){
            std::unique_lock<std::mutex> cv_lock(mgr->wake_up_mutex);
            mgr->cv.wait(cv_lock,[&mgr]{ return mgr->until(); });
        }

        bool until(){
            return !producer_empty.load();
        }
        
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
                        request_list_consumer = std::move(request_list_pending);
                        consumer_batches = std::move(producer_batches);

                        request_list_pending.clear();
                        producer_batches.clear();
                    }
                    // 无论如何迅速走开
                    consumer_empty.store(false, std::memory_order_release);
                    producer_empty.store(true, std::memory_order_release);
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