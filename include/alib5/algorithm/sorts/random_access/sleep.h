/**
 * @file sleep.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 睡眠排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_SLEEP
#define ALIB5_AALGO_SORT_SLEEP
#include <alib5/algorithm/sorts/base.h>
#include <chrono>
#include <thread>
#include <condition_variable>

namespace alib5::algo::sort{
    /**
     * @brief [R] Sleep sort. Literally uses actual system thread sleeping.
     * / 睡眠排序,真的用的是系统睡眠
     */
    template<class TimerBasic = std::chrono::milliseconds,
             size_t MultiplyBase = 1,
             IsRandomAccessIterator IterType> 
    void sleep(IterType begin, IterType end, bool reverse = false){
        static_assert(requires{ (size_t)(*begin); }, "Data must be able to be casted to double!");
        if (begin == end) return;

        std::mutex mtx;
        bool ready = false;
        std::condition_variable cv;
        std::vector<std::jthread> threads;
        IterType fill = reverse ? end - 1 : begin;

        for (IterType a = begin; a != end; ++a) {
            threads.emplace_back([reverse, &fill, time = (size_t)(*a), data = std::move(*a), &ready, &cv, &mtx] {
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [&ready] { return ready; });
                }
                std::this_thread::sleep_for(TimerBasic((size_t)time * MultiplyBase));
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    *fill = std::move(data);
                    if (reverse) --fill;
                    else ++fill;
                }
            });
        }
        {
            std::unique_lock<std::mutex> lock(mtx);
            ready = true;
            cv.notify_all();
        }
    }
}

#endif