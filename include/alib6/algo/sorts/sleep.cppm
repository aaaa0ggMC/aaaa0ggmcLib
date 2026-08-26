/**
 * @file sleep.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 睡眠排序 (alib6.algo:sort.sleep)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <iterator>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>

export module alib6.algo:sort.sleep;

import :base;

export namespace alib6::algo {

    template<
        class TimerBasic = std::chrono::milliseconds,
        std::size_t MultiplyBase = 1,
        IsRandomAccessIterator IterType
    > 
    void sleep_sort(IterType begin, IterType end, bool reverse = false) {
        static_assert(requires { static_cast<std::size_t>(*begin); }, "Data must be convertible to std::size_t!");
        if (begin == end) return;

        std::mutex mtx;
        bool ready = false;
        std::condition_variable cv;
        std::vector<std::jthread> threads;
        IterType fill = reverse ? end - 1 : begin;

        for (IterType a = begin; a != end; ++a) {
            threads.emplace_back([reverse, &fill, time = static_cast<std::size_t>(*a), data = *a, &ready, &cv, &mtx] {
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [&ready] { return ready; });
                }
                std::this_thread::sleep_for(TimerBasic(time * MultiplyBase));
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

} // namespace alib6::algo
