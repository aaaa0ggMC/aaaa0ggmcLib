/**
 * @file manager.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 多生产者/多消费者批量窃取请求管理器 (alib6.request:manager)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <list>
#include <deque>
#include <variant>
#include <concepts>
#include <utility>

export module alib6.request:manager;

import alib6.core;

namespace pmr = std::pmr;

export namespace alib6 {

    /// @brief 默认批次窃取阈值大小
    constexpr std::size_t conf_steal_batch_size = 32;

    /**
     * @brief 约束请求处理类型的概念
     */
    template<class T, class Ctx>
    concept IsRequestType = requires(T& t, Ctx& tx) {
        t.handle_request(tx);
    };

    /**
     * @brief 多生产者 / 多消费者批量窃取请求管理器
     */
    template<class ContextType, IsRequestType<ContextType>... RequestTypes>
    struct RequestManager {
        using queue_t = pmr::list<std::variant<std::monostate, RequestTypes...>>;
        using batch_pos_t = pmr::deque<typename queue_t::iterator>;

        std::mutex consumer_mutex;
        std::mutex wake_up_mutex;
        queue_t request_list_consumer;
        batch_pos_t consumer_batches;
        std::atomic<bool> consumer_empty{true};

        std::mutex producer_mutex;
        batch_pos_t producer_batches;
        queue_t request_list_pending;
        std::atomic<bool> producer_empty{true};

        std::condition_variable cv;

        explicit RequestManager(pmr::memory_resource* mem = get_default_resource())
            : request_list_consumer(mem)
            , consumer_batches(mem)
            , producer_batches(mem)
            , request_list_pending(mem) {}

        static void _wait_for_cv(RequestManager* mgr) {
            std::unique_lock<std::mutex> cv_lock(mgr->wake_up_mutex);
            mgr->cv.wait(cv_lock, [&mgr] { return mgr->until(); });
        }

        [[nodiscard]] bool until() const noexcept {
            return !producer_empty.load(std::memory_order_acquire);
        }

        /**
         * @brief 生产者：投递请求并根据批次边界唤醒消费者
         */
        template<class RequestType, class... Args>
        void push_request(Args&&... args) {
            std::size_t sz = 0;
            {
                std::lock_guard<std::mutex> lock(producer_mutex);
                request_list_pending.emplace_back().template emplace<RequestType>(
                    std::forward<Args>(args)...
                );
                sz = request_list_pending.size();
                if (sz % conf_steal_batch_size == 0) {
                    producer_batches.emplace_back(std::prev(request_list_pending.end()));
                }
            }
            if (producer_empty.load(std::memory_order_relaxed)) {
                producer_empty.store(false, std::memory_order_release);
                if (sz <= conf_steal_batch_size) cv.notify_one();
                else cv.notify_all();
            }
        }

        /**
         * @brief 单次尝试窃取并消费一个可用批次（非阻塞/单批次处理）
         * @return 实际消费的请求数量
         */
        std::size_t process_batch(ContextType& context) {
            queue_t local_queue(request_list_consumer.get_allocator().resource());
            
            // step 1: 如果 consumer 为空，尝试从 producer 整体 swap 偷取
            if (consumer_empty.load()) {
                if (producer_empty.load()) {
                    return 0;
                }
                std::scoped_lock locks(producer_mutex, consumer_mutex);
                if (consumer_empty.load()) {
                    request_list_consumer.swap(request_list_pending);
                    consumer_batches.swap(producer_batches);

                    request_list_pending.clear();
                    producer_batches.clear();

                    consumer_empty.store(false, std::memory_order_release);
                    producer_empty.store(true, std::memory_order_release);
                }
            }

            // step 2: 从 consumer 队列窃取一个批次至 local_queue
            {
                std::lock_guard<std::mutex> lock(consumer_mutex);
                if (!request_list_consumer.empty()) {
                    typename queue_t::iterator end_pos = request_list_consumer.end();
                    if (!consumer_batches.empty()) {
                        end_pos = consumer_batches.front();
                        consumer_batches.pop_front();
                    }
                    local_queue.splice(
                        local_queue.end(),
                        request_list_consumer,
                        request_list_consumer.begin(),
                        end_pos
                    );
                } else {
                    consumer_empty.store(true, std::memory_order_release);
                    return 0;
                }
            }

            // step 3: 派发执行
            std::size_t processed = 0;
            while (!local_queue.empty()) {
                auto& value = local_queue.front();
                std::visit([&context](auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (IsRequestType<T, ContextType>) {
                        v.handle_request(context);
                    }
                }, value);
                local_queue.pop_front();
                ++processed;
            }
            return processed;
        }

        /**
         * @brief 循环消费请求（阻塞直到停止或委托给 wait_fn）
         */
        template<class WaitFn = decltype(_wait_for_cv)>
            requires requires(WaitFn&& fn, RequestManager* rq) { fn(rq); }
        void handle_request(ContextType context, WaitFn&& fn = _wait_for_cv) {
            queue_t local_queue(request_list_consumer.get_allocator().resource());
            while (true) {
                if (consumer_empty.load()) {
                    if (producer_empty.load()) {
                        fn(this);
                    }
                    {
                        std::scoped_lock locks(producer_mutex, consumer_mutex);
                        if (consumer_empty.load()) {
                            request_list_consumer.swap(request_list_pending);
                            consumer_batches.swap(producer_batches);

                            request_list_pending.clear();
                            producer_batches.clear();

                            consumer_empty.store(false, std::memory_order_release);
                            producer_empty.store(true, std::memory_order_release);
                        }
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(consumer_mutex);
                    if (!request_list_consumer.empty()) {
                        typename queue_t::iterator end_pos = request_list_consumer.end();
                        if (!consumer_batches.empty()) {
                            end_pos = consumer_batches.front();
                            consumer_batches.pop_front();
                        }
                        local_queue.splice(
                            local_queue.end(),
                            request_list_consumer,
                            request_list_consumer.begin(),
                            end_pos
                        );
                    } else {
                        consumer_empty.store(true, std::memory_order_release);
                        continue;
                    }
                }
                while (!local_queue.empty()) {
                    auto& value = local_queue.front();
                    std::visit([&context](auto& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (IsRequestType<T, ContextType>) {
                            v.handle_request(context);
                        }
                    }, value);
                    local_queue.pop_front();
                }
            }
        }
    };

} // namespace alib6
