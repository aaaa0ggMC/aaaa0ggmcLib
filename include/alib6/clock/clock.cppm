/**
 * @file clock.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 高精度时钟与调度子系统聚合导出模块 (alib6.clock)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.clock;

export import :core;
export import :limiter;
export import :nap;

export namespace alib6 {
    namespace time {
        using alib6::Clock;
        using alib6::Trigger;
        using alib6::RateLimiter;
        using alib6::Timer;
        using alib6::nap;
        using alib6::nap0;
    }
}
