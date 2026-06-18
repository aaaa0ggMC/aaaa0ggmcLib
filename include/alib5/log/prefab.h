/**
 * @file prefab.h
 * @brief Prebuilt internal logger, factory and the `aout` streaming proxy.
 * 提供内置 logger、无附加信息的 LogFactory 以及 aout 流式输出代理。
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef ALIB5_ALOGGER_PREFAB
#define ALIB5_ALOGGER_PREFAB
#include <alib5/log/kernel.h>
#include <alib5/log/targets.h>

namespace alib5{
    /**
     * @brief Access the process-wide internal Logger instance.
     *
     * @par Original Comment:
     * 返回内置的logger
     *
     * @return Reference to the singleton internal Logger.
     */
    Logger& ALIB5_API __get_internal_logger();

    /// @brief Reference to the internal singleton logger.
    inline static Logger& alogger = __get_internal_logger();

    /// @brief Thread-shared LogFactory writing to the internal logger without extra info.
    inline static LogFactory __aout (__get_internal_logger(),[]{
        LogFactoryConfig cfg;
        cfg.msg.disable_extra_information = true;
        return cfg;
    }());

    /**
     * @brief Acquire the thread-local streamed context for `aout`.
     *
     * On each call where @p auto_create is true and the context was already used,
     * the stored context is destroyed and re-constructed to give a fresh line.
     *
     * @param auto_create When true, recycle the context if it has been used.
     * @return A moved (rvalue) streamed context for immediate chaining.
     */
    inline StreamedContext<LogFactory>&& _aout(bool auto_create = true){
        thread_local static StreamedContext<LogFactory> context = __aout();
        if(auto_create && context.context_used){
            context.~StreamedContext();
            new (&context) StreamedContext(__aout());
        }
        return std::move(context);
    }

    /**
     * @brief Proxy object backing the `aout` streaming macro/object.
     *
     * Forwards `operator<<` to a freshly obtained streamed context, preserving
     * the rvalue-ness of the context so manipulators keep chaining correctly.
     * On destruction it emits `endlog` if the context was never flushed.
     */
    struct _AOUT_PROXY {
        /**
         * @brief Stream @p t into the `aout` context.
         * @tparam T Value type being streamed.
         * @param t Value to forward to the context.
         * @return The (possibly moved) result of the underlying stream operation.
         */
        template<class T> decltype(auto) operator<<(T && t){
            using type = decltype(_aout() << std::forward<T>(t));
            if constexpr(std::is_same_v<type,StreamedContext<LogFactory>&&>) {
                return std::move(_aout() << std::forward<T>(t));
            }else {
                return _aout() << std::forward<T>(t);
            }
        }

        /// @brief Flush the current context, emitting any pending content.
        void flush(){
            auto && ctx = _aout();
            std::move(ctx) << fls;
        }

        /// @brief On destruction, finalize the context if it was never flushed.
        ~_AOUT_PROXY(){
            auto && ctx = _aout(false);
            if(!ctx.context_used && ctx.context_valid)std::move(ctx) << endlog;
        }
    };

    /// @brief Global streaming proxy used as `aout << ... << endlog;`.
    inline static _AOUT_PROXY aout;
}


#endif