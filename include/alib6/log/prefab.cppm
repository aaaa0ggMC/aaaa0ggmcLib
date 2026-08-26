/**
 * @file prefab.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 预制日志系统与开箱即用的全局 aout 流式输出代理
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.log.prefab;
import std;
import alib6.core;
import alib6.log;

export namespace alib6::log::prefab {

    /**
     * @brief 获取进程全局唯一的默认 Logger 实例 (预注册了标准控制台彩色输出目标)
     */
    Logger& get_default_logger();

    /// @brief 默认单例 Logger 引用
    inline Logger& alogger = get_default_logger();

    /// @brief 获取默认无前缀直接输出的全局 LogFactory 引用
    LogFactory& get_default_aout_factory();

    inline LogFactory& __aout = get_default_aout_factory();

    /**
     * @brief 获取线程局部的 StreamedContext 缓冲
     */
    StreamedContext<LogFactory>&& _aout(bool auto_create = true);

    /**
     * @brief 全局流式输出代理对象 aout
     * 支持 aout << "hello " << 42 << endlog; 形式的极简日志与控制台输出。
     */
    struct AoutProxy {
        template<class T>
        decltype(auto) operator<<(T&& t) const {
            using type = decltype(_aout() << std::forward<T>(t));
            if constexpr (std::is_same_v<type, StreamedContext<LogFactory>&&>) {
                return std::move(_aout() << std::forward<T>(t));
            } else {
                return _aout() << std::forward<T>(t);
            }
        }

        void flush() const {
            auto&& ctx = _aout();
            std::move(ctx) << fls;
        }

        ~AoutProxy() {
            auto&& ctx = _aout(false);
            if (!ctx.context_used && ctx.context_valid) {
                std::move(ctx) << endlog;
            }
        }
    };

    /// @brief 开箱即用的流式全局输出对象
    inline AoutProxy aout{};

} // namespace alib6::log::prefab

export namespace alib6 {
    using alib6::log::prefab::get_default_logger;
    using alib6::log::prefab::alogger;
    using alib6::log::prefab::aout;
    using alib6::log::prefab::_aout;
    using alib6::log::prefab::AoutProxy;
} // namespace alib6
