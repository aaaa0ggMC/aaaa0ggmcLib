/**
 * @file defer.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 延迟执行管理器
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:defer;
import std;
import :memory;

export namespace alib6 {

    /**
     * @brief 延迟执行管理器（支持 PMR 内存池分配与批量生命周期析构）
     */
    struct DeferManager {
    private:
        std::pmr::deque<std::move_only_function<void()>> defer_mgr;
    public:
        explicit DeferManager(memory_resource* mem = get_default_resource())
            : defer_mgr(mem) {}

        void defer(std::move_only_function<void()> function) {
            defer_mgr.emplace_back(std::move(function));
        }

        void clear() noexcept {
            defer_mgr.clear();
        }

        ~DeferManager() noexcept {
            while (!defer_mgr.empty()) {
                auto fn = std::move(defer_mgr.back());
                defer_mgr.pop_back();
                if (fn) {
                    try {
                        fn();
                    } catch (...) {
                        // 忽略析构异常
                    }
                }
            }
        }
    };

} // namespace alib6
