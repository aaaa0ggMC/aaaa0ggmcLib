/**
 * @file co.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 协程与并发同步子系统聚合导出模块 (alib6.co)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.co;

export import :concepts;
export import :task;
export import :sync;
export import :algo;

export namespace alib6 {
    namespace co = alib6::co;
    using alib6::co::Task;
    using alib6::co::Signal;
    using alib6::co::WaitGroup;
    using alib6::co::RepetitiveWork;
    using alib6::co::ThreadingWork;
    using alib6::co::Race;
}
