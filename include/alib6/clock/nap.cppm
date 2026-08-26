/**
 * @file nap.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 协程休眠生成器 (nap, nap0) (alib6.clock:nap)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <chrono>
#include <thread>
#include <generator>

export module alib6.clock:nap;

export namespace alib6 {

    inline std::generator<int> nap(std::chrono::nanoseconds nap_time) {
        while (true) {
            std::this_thread::sleep_for(nap_time);
            co_yield 0;
        }
    }

    inline std::generator<int> nap(double millisecs) {
        auto t = std::chrono::nanoseconds(static_cast<std::size_t>(millisecs * 1'000'000));
        while (true) {
            std::this_thread::sleep_for(t);
            co_yield 0;
        }
    }

    inline std::generator<int> nap0() {
        while (true) {
            std::this_thread::yield();
            co_yield 0;
        }
    }

} // namespace alib6
