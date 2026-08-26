/**
 * @file mod.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 日志管道模块基础接口 (输出目标 LogTarget 与过滤器 LogFilter 抽象基类及组合器)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.log:mod;
import std;
import alib6.core;
import :config;
import :msg;

export namespace alib6::log {

    /**
     * @brief 日志输出目标抽象接口 (负责向控制台、文件、网络、内存缓冲等目标进行持久化写入)
     */
    struct LogTarget {
        bool enabled{true};

        LogTarget& toggle(bool val) noexcept {
            enabled = val;
            return *this;
        }

        LogTarget() = default;

        /**
         * @brief 写入单条日志消息
         */
        virtual void write(LogMsg& msg) = 0;

        /**
         * @brief 刷新输出缓冲区
         */
        virtual void flush() {}

        /**
         * @brief 关闭 IO 资源
         */
        virtual void close() {}

        virtual ~LogTarget() {
            flush();
            close();
        }
    };

    /**
     * @brief 日志过滤器抽象接口 (支持对日志消息的放行判定或元数据修改)
     */
    struct LogFilter {
        bool enabled{true};

        LogFilter& toggle(bool val) noexcept {
            enabled = val;
            return *this;
        }

        LogFilter() = default;

        /**
         * @brief 消费端过滤逻辑 (返回 true 放行，false 拦截丢弃)
         */
        virtual bool filter(LogMsg& msg) {
            return true;
        }

        /**
         * @brief 生产端提前过滤逻辑 (在消息主体构建前进行初步判定)
         */
        virtual bool pre_filter(int level, std::string_view raw_message, const LogMsgConfig& cfg) {
            return true;
        }

        virtual void close() {}

        virtual ~LogFilter() {
            close();
        }
    };

    template<class T>
    concept IsLogTarget = std::derived_from<T, LogTarget>;

    template<class T>
    concept IsLogFilter = std::derived_from<T, LogFilter>;

    template<class T>
    concept IsLogMod = IsLogTarget<T> || IsLogFilter<T>;

    /**
     * @brief 复合日志输出目标 (将日志写入广播至包含的所有目标)
     */
    template<IsLogTarget... Ts>
    struct LogTargetGroup : public LogTarget {
        std::tuple<Ts...> targets;

        explicit LogTargetGroup(std::tuple<Ts...>&& t) : targets(std::move(t)) {}
        explicit LogTargetGroup(Ts&&... args) : targets(std::forward<Ts>(args)...) {}

        template<usize N>
        void toggle_target(bool val = true) noexcept {
            std::get<N>(targets).enabled = val;
        }

        void toggle_all(bool val) noexcept {
            std::apply([val](auto&... t) {
                ((t.enabled = val), ...);
            }, targets);
        }

        void write(LogMsg& msg) override {
            std::apply([&msg](auto&... t) {
                ((t.enabled ? t.write(msg) : void()), ...);
            }, targets);
        }

        void flush() override {
            std::apply([](auto&... t) {
                ((t.enabled ? t.flush() : void()), ...);
            }, targets);
        }
    };

    /**
     * @brief 复合日志过滤器 (AND 方式结合所有子过滤器的判决结果)
     */
    template<IsLogFilter... Ts>
    struct LogFilterGroup : public LogFilter {
        std::tuple<Ts...> filters;

        explicit LogFilterGroup(std::tuple<Ts...>&& t) : filters(std::move(t)) {}
        explicit LogFilterGroup(Ts&&... args) : filters(std::forward<Ts>(args)...) {}

        template<usize N>
        void toggle_filter(bool val = true) noexcept {
            std::get<N>(filters).enabled = val;
        }

        void toggle_all(bool val) noexcept {
            std::apply([val](auto&... t) {
                ((t.enabled = val), ...);
            }, filters);
        }

        bool filter(LogMsg& msg) override {
            bool ok = true;
            std::apply([&ok, &msg](auto&... f) {
                ((ok = ok && (f.enabled ? f.filter(msg) : true)), ...);
            }, filters);
            return ok;
        }

        bool pre_filter(int level, std::string_view raw_message, const LogMsgConfig& cfg) override {
            bool ok = true;
            std::apply([&ok, level, raw_message, &cfg](auto&... f) {
                ((ok = ok && (f.enabled ? f.pre_filter(level, raw_message, cfg) : true)), ...);
            }, filters);
            return ok;
        }
    };

} // namespace alib6::log
