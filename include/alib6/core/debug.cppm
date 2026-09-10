/**
 * @file debug.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 调试、轻量断言与 Panic 支持 (现代 C++23 / 模块化迁移)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cstdio>
#include <version>

export module alib6.core:debug;
import std;
import :types;

namespace alib6::detail {

#if defined(__cpp_lib_stacktrace) && !defined(ALIB6_FLAG_NO_STACKTRACE)
    /**
     * @brief 简化并格式化调用栈（消除冗余堆分配与 C++ 内部符号）
     */
    inline std::string simplify_stacktrace(const std::stacktrace& st) {
        std::string ret;
        constexpr usize skipc = 2; // 跳过 panic 本身的两层内部栈
        usize index = 0;

        for (const auto& entry : st) {
            if (index < skipc) {
                ++index;
                continue;
            }
            if (entry.source_file().empty() && entry.source_line() == 0) {
                ++index;
                continue;
            }

            if (entry.source_file().find(ALIB6_TRACE_SKIP_PROMPT) != std::string_view::npos) {
                ++index;
                continue;
            }

            std::format_to(
                std::back_inserter(ret),
                "  {}# {}:{} at {}\n",
                index - skipc,
                entry.source_file(),
                entry.source_line(),
                entry.description()
            );
            ++index;
        }
        return ret;
    }
#endif

    /**
     * @brief 底层 panic 终止逻辑
     */
    [[noreturn]] inline void internal_panic(
        std::string_view msg,
        const std::source_location& sl = std::source_location::current()
    ) {
#if defined(__cpp_lib_stacktrace) && !defined(ALIB6_FLAG_NO_STACKTRACE)
        std::string trace = simplify_stacktrace(std::stacktrace::current(1));
#else
        std::string_view trace = "<Stacktrace Disabled>";
#endif

        constexpr std::string_view panic_fmt = 
R"(Message  : {}
Source   : {}:{}:{}
Function : {}
Stack    : 
{}
)";

        std::string final_str = std::format(
            panic_fmt,
            msg,
            sl.file_name(),
            sl.line(),
            sl.column(),
            sl.function_name(),
            trace
        );

#ifndef ALIB6_FLAG_NO_DEBUG_OUTPUT
        // 使用 std::println 保证单次原子输出，避免多线程输出交织撕裂
        std::println(stderr, "{}", final_str);
#endif

#if defined(ALIB6_FLAG_USE_EXCEPTIONS)
        throw std::runtime_error(final_str);
#else
        std::abort();
#endif
    }

} // namespace alib6::detail

export namespace alib6 {

    /**
     * @brief 带源码位置捕获的格式化字符串封装（允许在函数调用时无宏捕获 source_location）
     */
    template<class... Args>
    struct PanicFormat {
        std::format_string<Args...> fmt;
        std::source_location loc;

        template<class Str>
            requires std::constructible_from<std::format_string<Args...>, const Str&>
        consteval PanicFormat(const Str& str, std::source_location loc = std::source_location::current()) noexcept
            : fmt(str), loc(loc) {}
    };

    /**
     * @brief 触发 Panic（单参数 / 纯文本）
     */
    [[noreturn]] inline void panic(
        std::string_view msg,
        std::source_location loc = std::source_location::current()
    ) {
        detail::internal_panic(msg, loc);
    }

    /**
     * @brief 触发 Panic（std::format 格式化）
     * @note 借助 PanicFormat，无需任何宏包装即可自动捕获调用点源码文件名与行号
     */
    template<class... Args>
    [[noreturn]] void panicf(
        PanicFormat<std::type_identity_t<Args>...> fmt,
        Args&&... args
    ) {
        std::string message = std::format(fmt.fmt, std::forward<Args>(args)...);
        detail::internal_panic(message, fmt.loc);
    }

    /**
     * @brief 动态运行时格式串 panic（类似 vformat）
     */
    template<class... Args>
    [[noreturn]] void vpanicf(
        std::string_view fmt,
        std::source_location loc,
        Args&&... args
    ) {
        std::string message = std::vformat(fmt, std::make_format_args(args...));
        detail::internal_panic(message, loc);
    }

} // namespace alib6
