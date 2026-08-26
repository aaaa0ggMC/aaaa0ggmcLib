/**
 * @file stream.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 日志流式上下文 StreamedContext 与操作符重载管道 (支持编译期注入、格式化、Tag 标签与参数绑定)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>
#include <charconv>

export module alib6.log:stream;
import std;
import alib6.core;
import :config;
import :msg;
import :manip;
import :fastfmt;

namespace pmr = std::pmr;

export namespace alib6::log {

    template<class T>
    concept GoUniversal = std::formattable<T, char>;

    template<class T>
    concept CanForward = requires(T&& t, pmr::string& target) {
        t.write_to_log(target);
    } || requires(T&& t, pmr::string& target) {
        write_to_log(target, t);
    };

    template<class T>
    concept CanManipulate = requires(T&& t, LogMsgConfig& c) {
        t.manipulate(c);
    };

    template<class T, class Context>
    concept CanSelfForward = requires(T&& t, Context&& ctx) {
        { std::forward<T>(t).self_forward(std::move(ctx)) } -> std::same_as<Context&&>;
    };

    /**
     * @brief 仅可移动的流式日志收集上下文
     * 通过连续的 operator<< 链式聚合数据，在析构或收到 endlog / fls 时触发 upload() 提交。
     */
    template<class LogFactory>
    struct StreamedContext {
        LogFactory& factory;
        pmr::string cache_str;
        int level{0};
        bool context_valid{true};
        bool context_used{false};
        std::string_view fmt_str{""};
        bool fmt_tmp{false};
        bool fmt_locked{false};
        LogMsgConfig msg_cfg{};
        pmr::vector<LogCustomTag> tags;

        struct FMTLock {
            bool& v;
            explicit FMTLock(bool& x) : v(x) { v = true; }
            ~FMTLock() { v = false; }
        };

        StreamedContext(const StreamedContext&) = delete;
        StreamedContext& operator=(const StreamedContext&) = delete;
        StreamedContext(StreamedContext&&) noexcept = default;
        StreamedContext& operator=(StreamedContext&&) noexcept = default;

        [[nodiscard]] FMTLock lock_fmt() noexcept {
            return FMTLock(fmt_locked);
        }

        void try_restore_fmt() noexcept {
            if (fmt_tmp && !fmt_locked) {
                fmt_str = "";
                fmt_tmp = false;
            }
        }

        StreamedContext(int lvl, LogFactory& fac, bool valid = true)
            : factory(fac)
            , cache_str(fac.get_allocator())
            , level(lvl)
            , context_valid(valid)
            , context_used(false)
            , msg_cfg(fac.get_msg_config())
            , tags(fac.get_allocator()) {}

        ~StreamedContext() {
            if (!context_used && context_valid && !cache_str.empty()) {
                std::move(*this).upload();
            }
        }

        /**
         * @brief 提交当前日志行并触发 Logger 消费
         */
        bool upload() && {
            if (context_used) return false;
            context_used = true;
            if (context_valid && !cache_str.empty()) {
                return factory.log_pmr(level, std::move(cache_str), msg_cfg, tags);
            }
            return false;
        }

        template<class T>
        StreamedContext&& write(T&& t) && {
            if (!context_valid) return std::move(*this);
            if (fmt_str.empty()) {
                using DecayT = std::decay_t<T>;
                if constexpr (std::same_as<DecayT, std::nullptr_t>) {
                    cache_str.append("nullptr");
                } else if constexpr (std::same_as<DecayT, bool>) {
                    cache_str.append(t ? "true" : "false");
                } else if constexpr (std::same_as<DecayT, char>) {
                    cache_str.push_back(t);
                } else if constexpr (std::convertible_to<T, std::string_view>) {
                    cache_str.append(std::forward<T>(t));
                } else if constexpr (std::is_pointer_v<DecayT>) {
                    if (!t) {
                        cache_str.append("nullptr");
                    } else {
                        cache_str.append("0x");
                        char buf[32];
                        auto res = std::to_chars(buf, buf + sizeof(buf), reinterpret_cast<std::uintptr_t>(t), 16);
                        cache_str.append(buf, static_cast<usize>(res.ptr - buf));
                    }
                } else if constexpr (std::integral<DecayT>) {
                    char cbuf[32];
                    auto res = std::to_chars(cbuf, cbuf + sizeof(cbuf), t);
                    cache_str.append(cbuf, static_cast<usize>(res.ptr - cbuf));
                } else if constexpr (std::floating_point<DecayT>) {
                    char cbuf[64];
                    auto res = std::to_chars(cbuf, cbuf + sizeof(cbuf), t);
                    cache_str.append(cbuf, static_cast<usize>(res.ptr - cbuf));
                } else if constexpr (requires(pmr::string& s, const T& v) { write_to_log(s, v); }) {
                    write_to_log(cache_str, t);
                } else {
                    std::format_to(std::back_inserter(cache_str), "{}", t);
                }
            } else {
                std::vformat_to(
                    std::back_inserter(cache_str),
                    fmt_str,
                    std::make_format_args(t)
                );
                try_restore_fmt();
            }
            return std::move(*this);
        }

        template<class T>
            requires (CanForward<T> && !CanSelfForward<T, StreamedContext>)
        StreamedContext&& operator<<(T&& t) && {
            if (!context_valid) return std::move(*this);
            if constexpr (requires(T&& val, pmr::string& s) { write_to_log(s, val); }) {
                write_to_log(cache_str, t);
            } else {
                t.write_to_log(cache_str);
            }
            return std::move(*this);
        }

        template<class T>
            requires (!CanSelfForward<T, StreamedContext> && !CanForward<T> && std::convertible_to<T, std::string_view>)
        StreamedContext&& operator<<(T&& t) && {
            if (!context_valid) return std::move(*this);
            if (fmt_str.empty()) {
                cache_str.append(std::string_view(t));
            } else {
                std::vformat_to(std::back_inserter(cache_str), fmt_str, std::make_format_args(t));
                try_restore_fmt();
            }
            return std::move(*this);
        }

        StreamedContext&& operator<<(bool v) && {
            if (!context_valid) return std::move(*this);
            if (fmt_str.empty()) {
                cache_str.append(v ? "true" : "false");
            } else {
                std::vformat_to(std::back_inserter(cache_str), fmt_str, std::make_format_args(v));
                try_restore_fmt();
            }
            return std::move(*this);
        }

        StreamedContext&& operator<<(char ch) && {
            if (!context_valid) return std::move(*this);
            if (fmt_str.empty()) {
                cache_str.push_back(ch);
            } else {
                std::vformat_to(std::back_inserter(cache_str), fmt_str, std::make_format_args(ch));
                try_restore_fmt();
            }
            return std::move(*this);
        }

        template<std::integral T>
            requires (!std::same_as<std::decay_t<T>, bool> && !std::same_as<std::decay_t<T>, char>)
        StreamedContext&& operator<<(T v) && {
            if (!context_valid) return std::move(*this);
            if (fmt_str.empty()) {
                char buf[32];
                auto res = std::to_chars(buf, buf + sizeof(buf), v);
                cache_str.append(buf, static_cast<usize>(res.ptr - buf));
            } else {
                std::vformat_to(std::back_inserter(cache_str), fmt_str, std::make_format_args(v));
                try_restore_fmt();
            }
            return std::move(*this);
        }

        template<std::floating_point T>
        StreamedContext&& operator<<(T v) && {
            if (!context_valid) return std::move(*this);
            if (fmt_str.empty()) {
                char buf[64];
                auto res = std::to_chars(buf, buf + sizeof(buf), v);
                cache_str.append(buf, static_cast<usize>(res.ptr - buf));
            } else {
                std::vformat_to(std::back_inserter(cache_str), fmt_str, std::make_format_args(v));
                try_restore_fmt();
            }
            return std::move(*this);
        }

        template<class T>
            requires (!CanSelfForward<T, StreamedContext> && !CanForward<T> && !std::convertible_to<T, std::string_view> && !std::integral<std::decay_t<T>> && !std::floating_point<std::decay_t<T>> && GoUniversal<T>)
        StreamedContext&& operator<<(T&& t) && {
            return std::move(*this).template write<T>(std::forward<T>(t));
        }

        template<class T1, class T2>
            requires (!CanSelfForward<std::pair<T1, T2>, StreamedContext> && !CanForward<std::pair<T1, T2>> && !GoUniversal<std::pair<T1, T2>>)
        StreamedContext&& operator<<(const std::pair<T1, T2>& p) && {
            if (!context_valid) return std::move(*this);
            cache_str.push_back('(');
            std::move(*this) << p.first;
            cache_str.append(", ");
            std::move(*this) << p.second;
            cache_str.push_back(')');
            return std::move(*this);
        }

        template<class T>
            requires (!CanSelfForward<std::optional<T>, StreamedContext> && !CanForward<std::optional<T>> && !GoUniversal<std::optional<T>>)
        StreamedContext&& operator<<(const std::optional<T>& opt) && {
            if (!context_valid) return std::move(*this);
            if (opt) {
                std::move(*this) << *opt;
            } else {
                cache_str.append("nullopt");
            }
            return std::move(*this);
        }

        template<class R>
            requires (!CanSelfForward<R, StreamedContext> && std::ranges::input_range<R> && !std::convertible_to<R, std::string_view> && !CanForward<R> && !GoUniversal<R>)
        StreamedContext&& operator<<(R&& r) && {
            if (!context_valid) return std::move(*this);
            cache_str.push_back('[');
            bool first = true;
            for (auto&& elem : r) {
                if (!first) cache_str.append(", ");
                first = false;
                std::move(*this) << elem;
            }
            cache_str.push_back(']');
            return std::move(*this);
        }

        template<CanManipulate T>
        StreamedContext&& operator<<(T&& t) && {
            if (!context_valid) return std::move(*this);
            t.manipulate(msg_cfg);
            return std::move(*this);
        }

        template<class T>
        StreamedContext&& operator<<(log_omit<T>&& val) && {
            if (!context_valid) return std::move(*this);
            usize clen = cache_str.size();
            std::move(*this) << std::forward<T>(val.v);
            usize clen2 = cache_str.size();

            if (clen + val.max_length < clen2) {
                cache_str.resize(clen + val.max_length);
                if (val.need_fmt) {
                    usize omitted_bytes = clen2 - clen - val.max_length;
                    std::vformat_to(
                        std::back_inserter(cache_str),
                        val.omit_str,
                        std::make_format_args(omitted_bytes)
                    );
                } else {
                    cache_str.append(val.omit_str);
                }
            }
            return std::move(*this);
        }

        StreamedContext&& operator<<(log_nop) && noexcept {
            return std::move(*this);
        }

        StreamedContext&& operator<<(log_erase fn) && {
            if (fn.count >= cache_str.size()) {
                tags.clear();
                cache_str.clear();
            } else {
                cache_str.resize(cache_str.size() - fn.count);
                while (!tags.empty() && tags.back().get_pos() > cache_str.size()) {
                    tags.pop_back();
                }
            }
            return std::move(*this);
        }

        bool operator<<(LogEnd) && {
            return std::move(*this).upload();
        }

        bool operator|(LogEnd) && {
            return std::move(*this).upload();
        }

        bool operator<<(std::ostream& (*)(std::ostream&)) && {
            return std::move(*this).upload();
        }

        StreamedContext&& operator<<(const log_tag& t) && {
            if (!context_valid) return std::move(*this);
            LogCustomTag& tag = tags.emplace_back(t.category, t.payload);
            tag.set_pos(cache_str.size());
            return std::move(*this);
        }

        template<CanSelfForward<StreamedContext> T>
        StreamedContext&& operator<<(T&& t) && {
            if (!context_valid) return std::move(*this);
            return std::forward<T>(t).self_forward(std::move(*this));
        }

        StreamedContext&& operator<<(const log_fmt& fmt) && noexcept {
            if (!context_valid) return std::move(*this);
            fmt_str = fmt.fmt_str;
            fmt_tmp = false;
            return std::move(*this);
        }

        StreamedContext&& operator<<(const log_tfmt& fmt) && noexcept {
            if (!context_valid) return std::move(*this);
            fmt_str = fmt.fmt_str;
            fmt_tmp = true;
            return std::move(*this);
        }
    };

    /**
     * @brief 绑定参数包便于一次性流式注入
     */
    template<typename... Ts>
    struct CoupledArgs {
        std::tuple<Ts...> storage;

        template<typename... Args>
        explicit CoupledArgs(Args&&... args) : storage(std::forward<Args>(args)...) {}

        template<class Context>
        Context&& self_forward(Context&& ctx) && {
            return std::apply([&ctx](auto&... args) mutable -> Context&& {
                return (std::move(ctx) << ... << args);
            }, storage);
        }
    };

    template<typename... Args>
    [[nodiscard]] auto couple(Args&&... args) {
        return CoupledArgs<std::decay_t<Args>...>(std::forward<Args>(args)...);
    }

} // namespace alib6::log
