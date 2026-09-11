/**
 * @file filters.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 内置日志过滤器 (CustomLevelBlocker 自定义级别拦截器等)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.log:filters;
import std;
import alib6.core;
import :config;
import :msg;
import :mod;

export namespace alib6::lof {
    using namespace alib6::log;

    /**
     * @brief 自定义级别过滤器 (委托给用户函数进行生产端快速剪枝)
     */
    struct CustomLevelBlocker : public LogFilter {
        using CustomFn = std::function<bool(int)>;

        CustomFn should_keep;

        explicit CustomLevelBlocker(CustomFn ishould_keep) : should_keep(std::move(ishould_keep)) {
            if (!should_keep) {
                should_keep = [](int) { return true; };
                toggle(false);
            }
        }

        bool pre_filter(int level, std::string_view, const LogMsgConfig&) override {
            return should_keep ? should_keep(level) : true;
        }
    };

    /**
     * @brief 最低级别过滤器 (拦截低于 min_level 的所有日志)
     */
    struct MinLevelFilter : public LogFilter {
        int min_level{0};

        explicit MinLevelFilter(int min_lvl = static_cast<int>(LogLevel::Info)) : min_level(min_lvl) {}
        explicit MinLevelFilter(LogLevel min_lvl) : min_level(static_cast<int>(min_lvl)) {}

        bool pre_filter(int level, std::string_view, const LogMsgConfig&) override {
            return level >= min_level;
        }
    };

} // namespace alib6::lof

export namespace alib6::log {
    namespace lof = alib6::lof;
    using alib6::lof::CustomLevelBlocker;
    using alib6::lof::MinLevelFilter;
} // namespace alib6::log
