/**
 * @file fastfmt.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 非侵入式高性能日志格式化扩展库 (alib6.log:fastfmt / alib6.log.fastfmt)
 * 提供针对常用标准库类型与 GLM 数学类型的零抽象开销 write_to_log 重载。
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <thread>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <span>
#include <tuple>
#include <utility>
#include <optional>
#include <variant>

#ifdef ALIB6_HAS_GLM
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#endif

export module alib6.log:fastfmt;
import std;
import alib6.core;

namespace pmr = std::pmr;

export namespace alib6::log {

    // ==========================================
    // 1. 指针与空指针 (Pointers & Nullptr)
    // ==========================================

    inline void write_to_log(pmr::string& target, std::nullptr_t) {
        target.append("nullptr");
    }

    template<class T>
        requires (!std::same_as<std::decay_t<T>, char> && !std::same_as<std::decay_t<T>, char8_t>)
    inline void write_to_log(pmr::string& target, const T* ptr) {
        if (!ptr) {
            target.append("nullptr");
            return;
        }
        char buf[32];
        buf[0] = '0';
        buf[1] = 'x';
        auto res = std::to_chars(buf + 2, buf + sizeof(buf), reinterpret_cast<std::uintptr_t>(ptr), 16);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
    }

    // ==========================================
    // 2. 线程 ID (std::thread::id)
    // ==========================================

    inline void write_to_log(pmr::string& target, std::thread::id tid) {
        target.append("TID:");
        char buf[32];
        auto id_val = static_cast<std::uint64_t>(std::hash<std::thread::id>{}(tid));
        auto res = std::to_chars(buf, buf + sizeof(buf), id_val, 16);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
    }

    // ==========================================
    // 3. 源码调用点 (std::source_location)
    // ==========================================

    inline void write_to_log(pmr::string& target, const std::source_location& loc) {
        target.append(loc.file_name());
        target.push_back(':');
        char lbuf[16];
        auto lres = std::to_chars(lbuf, lbuf + sizeof(lbuf), loc.line());
        target.append(lbuf, static_cast<std::size_t>(lres.ptr - lbuf));
        target.append(" in ");
        target.append(loc.function_name());
    }

    // ==========================================
    // 4. 时间与时长 (std::chrono::duration & time_point)
    // ==========================================

    template<class Rep, class Period>
    inline void write_to_log(pmr::string& target, const std::chrono::duration<Rep, Period>& d) {
        char buf[32];
        auto res = std::to_chars(buf, buf + sizeof(buf), d.count());
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));

        if constexpr (std::same_as<Period, std::nano>) {
            target.append("ns");
        } else if constexpr (std::same_as<Period, std::micro>) {
            target.append("us");
        } else if constexpr (std::same_as<Period, std::milli>) {
            target.append("ms");
        } else if constexpr (std::same_as<Period, std::ratio<1>>) {
            target.append("s");
        } else if constexpr (std::same_as<Period, std::ratio<60>>) {
            target.append("min");
        } else if constexpr (std::same_as<Period, std::ratio<3600>>) {
            target.append("h");
        } else {
            target.append(" [duration]");
        }
    }

    // ==========================================
    // 5. 文件路径 (std::filesystem::path)
    // ==========================================

    inline void write_to_log(pmr::string& target, const std::filesystem::path& p) {
        target.append(p.string());
    }

    // ==========================================
    // 6. 错误码 (std::error_code & error_condition)
    // ==========================================

    inline void write_to_log(pmr::string& target, const std::error_code& ec) {
        target.append(ec.category().name());
        target.push_back(':');
        char buf[16];
        auto res = std::to_chars(buf, buf + sizeof(buf), ec.value());
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(" (");
        target.append(ec.message());
        target.push_back(')');
    }

    inline void write_to_log(pmr::string& target, const std::error_condition& ec) {
        target.append(ec.category().name());
        target.push_back(':');
        char buf[16];
        auto res = std::to_chars(buf, buf + sizeof(buf), ec.value());
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(" (");
        target.append(ec.message());
        target.push_back(')');
    }

    // ==========================================
    // 7. 字节类型 (std::byte)
    // ==========================================

    inline void write_to_log(pmr::string& target, std::byte b) {
        target.append("0x");
        char buf[8];
        auto res = std::to_chars(buf, buf + sizeof(buf), std::to_integer<unsigned int>(b), 16);
        if (res.ptr - buf == 1) {
            target.push_back('0');
        }
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
    }

    // ==========================================
    // 8. 结构化容器 (pair, tuple, optional, variant)
    // ==========================================

    template<class T1, class T2>
    inline void write_to_log(pmr::string& target, const std::pair<T1, T2>& p) {
        target.push_back('(');
        if constexpr (requires(pmr::string& s, const T1& v) { write_to_log(s, v); }) {
            write_to_log(target, p.first);
        } else if constexpr (std::convertible_to<T1, std::string_view>) {
            target.append(std::string_view(p.first));
        } else if constexpr (std::integral<std::decay_t<T1>>) {
            char buf[32];
            auto res = std::to_chars(buf, buf + sizeof(buf), p.first);
            target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        } else {
            std::format_to(std::back_inserter(target), "{}", p.first);
        }

        target.append(", ");

        if constexpr (requires(pmr::string& s, const T2& v) { write_to_log(s, v); }) {
            write_to_log(target, p.second);
        } else if constexpr (std::convertible_to<T2, std::string_view>) {
            target.append(std::string_view(p.second));
        } else if constexpr (std::integral<std::decay_t<T2>>) {
            char buf[32];
            auto res = std::to_chars(buf, buf + sizeof(buf), p.second);
            target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        } else {
            std::format_to(std::back_inserter(target), "{}", p.second);
        }
        target.push_back(')');
    }

    template<class T>
    inline void write_to_log(pmr::string& target, const std::optional<T>& opt) {
        if (opt.has_value()) {
            if constexpr (requires(pmr::string& s, const T& v) { write_to_log(s, v); }) {
                write_to_log(target, *opt);
            } else if constexpr (std::convertible_to<T, std::string_view>) {
                target.append(std::string_view(*opt));
            } else if constexpr (std::integral<std::decay_t<T>>) {
                char buf[32];
                auto res = std::to_chars(buf, buf + sizeof(buf), *opt);
                target.append(buf, static_cast<std::size_t>(res.ptr - buf));
            } else {
                std::format_to(std::back_inserter(target), "{}", *opt);
            }
        } else {
            target.append("nullopt");
        }
    }

    inline void write_to_log(pmr::string& target, std::monostate) {
        target.append("monostate");
    }

    template<class... Ts>
    inline void write_to_log(pmr::string& target, const std::variant<Ts...>& var) {
        std::visit([&target](const auto& val) {
            using VT = std::decay_t<decltype(val)>;
            if constexpr (requires(pmr::string& s, const VT& v) { write_to_log(s, v); }) {
                write_to_log(target, val);
            } else if constexpr (std::convertible_to<VT, std::string_view>) {
                target.append(std::string_view(val));
            } else if constexpr (std::integral<VT>) {
                char buf[32];
                auto res = std::to_chars(buf, buf + sizeof(buf), val);
                target.append(buf, static_cast<std::size_t>(res.ptr - buf));
            } else {
                std::format_to(std::back_inserter(target), "{}", val);
            }
        }, var);
    }

    // ==========================================
    // 9. GLM 数学库向量与四元数快速格式化 (GLM Fast Formats)
    // ==========================================
#if defined(ALIB6_HAS_GLM)
    template<glm::length_t L, typename T, glm::qualifier Q>
    inline void write_to_log(pmr::string& target, const glm::vec<L, T, Q>& v) {
        target.append("vec");
        target.push_back(static_cast<char>('0' + L));
        target.push_back('(');
        for (glm::length_t i = 0; i < L; ++i) {
            if (i > 0) target.append(", ");
            if constexpr (std::integral<T>) {
                char buf[32];
                auto res = std::to_chars(buf, buf + sizeof(buf), v[i]);
                target.append(buf, static_cast<std::size_t>(res.ptr - buf));
            } else if constexpr (std::floating_point<T>) {
                char buf[64];
                auto res = std::to_chars(buf, buf + sizeof(buf), v[i]);
                target.append(buf, static_cast<std::size_t>(res.ptr - buf));
            } else {
                std::format_to(std::back_inserter(target), "{}", v[i]);
            }
        }
        target.push_back(')');
    }

    template<typename T, glm::qualifier Q>
    inline void write_to_log(pmr::string& target, const glm::qua<T, Q>& q) {
        target.append("quat(w=");
        char buf[64];
        auto res = std::to_chars(buf, buf + sizeof(buf), q.w);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(", x=");
        res = std::to_chars(buf, buf + sizeof(buf), q.x);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(", y=");
        res = std::to_chars(buf, buf + sizeof(buf), q.y);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(", z=");
        res = std::to_chars(buf, buf + sizeof(buf), q.z);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.push_back(')');
    }
#endif

} // namespace alib6::log
