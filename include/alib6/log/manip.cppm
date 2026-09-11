/**
 * @file manip.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 日志流式操作符与修饰器 (endlog, fls, log_source, log_bin, log_omit, log_rate, log_stacktrace 等)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>

export module alib6.log:manip;
import std;
import alib6.core;
import :config;

namespace pmr = std::pmr;

export namespace alib6::log {

    /// @brief 流式日志终止标记类型
    struct LogEnd {};

    /// @brief 终止并提交当前日志行
    inline constexpr LogEnd endlog{};
    /// @brief 刷新并提交当前日志行 (别名)
    inline constexpr LogEnd fls{};

    /// @brief 默认省略提示格式
    inline constexpr std::string_view default_omit_str = "...[+{} bytes]";

    /**
     * @brief 源代码位置注入修饰器 (自动捕获调用处 source_location)
     */
    struct log_source {
        std::source_location loc;
        bool keep_full;

        constexpr log_source(
            bool kf = false,
            std::source_location cloc = std::source_location::current()
        ) noexcept : loc(cloc), keep_full(kf) {}

        template<class Target>
        void write_to_log(Target& str) const {
            std::string_view p = loc.file_name();
            if (!keep_full) {
                auto pos = p.find_last_of("/\\");
                if (pos != std::string_view::npos && pos + 1 < p.size()) {
                    p = p.substr(pos + 1);
                }
            }
            std::format_to(std::back_inserter(str), "[{}:{} {}]", p, loc.line(), loc.function_name());
        }
    };

    enum class LogBinFormat : u8 {
        Bin = 1,
        Oct = 3,
        Hex = 4
    };

    struct LogBinConfig {
        LogBinFormat fmt{LogBinFormat::Hex};
        usize split_when{2};
        std::string_view split_str{" "};
        bool capital{false};
    };

    /**
     * @brief 二进制 / 十六进制 / 八进制数据格式化输出修饰器
     */
    struct log_bin {
        using Format = LogBinFormat;
        using Config = LogBinConfig;
        using cfg = LogBinConfig;

        static constexpr Format Bin = LogBinFormat::Bin;
        static constexpr Format Oct = LogBinFormat::Oct;
        static constexpr Format Hex = LogBinFormat::Hex;

        Config config{};
        const void* data{nullptr};
        usize size{0};

        template<class T>
        explicit log_bin(const T& val, Config c = Config{})
            : config(c), data(std::addressof(val)), size(sizeof(T)) {}

        log_bin(const void* ptr, usize s, Config c = Config{})
            : config(c), data(ptr), size(s) {}

        template<class T>
        explicit log_bin(std::span<const T> list, Config c = Config{})
            : config(c), data(list.data()), size(list.size_bytes()) {}

        template<class T>
        explicit log_bin(std::basic_string_view<T> sv, Config c = Config{})
            : config(c), data(sv.data()), size(sv.size() * sizeof(T)) {}

        void write_to_log(pmr::string& target) const;
    };

    /**
     * @brief 超长日志自动截断修饰器
     */
    template<class T>
    struct log_omit {
        T v;
        usize max_length;
        std::string_view omit_str;
        bool need_fmt;

        template<class R>
        log_omit(
            R&& iv,
            usize max_count,
            std::string_view omit = default_omit_str,
            bool ineed_fmt = true
        ) : v(std::forward<R>(iv))
          , max_length(max_count)
          , omit_str(omit)
          , need_fmt(ineed_fmt) {}

        log_omit(const log_omit&) = delete;
        log_omit& operator=(const log_omit&) = delete;
        log_omit(log_omit&&) = default;
        log_omit& operator=(log_omit&&) = default;
    };

    template<class R, class... Args>
    log_omit(R&& iv, Args&&...) -> log_omit<R>;

    /**
     * @brief 日志输出频次限制修饰器 (Rate Limit / Once)
     */
    struct log_rate {
        struct Info {
            enum Type { Rate, Once };
            Type type{Rate};
            std::atomic<bool> printed{false};
            std::atomic<double> last_time{0.0};
            double times_per_second{-1.0};
            std::chrono::steady_clock::time_point start_tp{std::chrono::steady_clock::now()};

            Info& set_type(Type t) noexcept { type = t; return *this; }
            Info& set_rate(double tps) noexcept { times_per_second = tps; return *this; }

            bool check() noexcept;
        };

        struct Context {
            pmr::unordered_map<usize, std::unique_ptr<Info>> mapping;
            std::mutex mutex;

            explicit Context(memory_resource* mem = get_default_resource())
                : mapping(mem) {}

            Info& get(usize id) {
                std::lock_guard<std::mutex> lock(mutex);
                auto it = mapping.find(id);
                if (it == mapping.end()) {
                    auto info = std::make_unique<Info>();
                    auto* ptr = info.get();
                    mapping.emplace(id, std::move(info));
                    return *ptr;
                }
                return *it->second;
            }
        };

        using context = Context;

        Info& info;

        log_rate(usize id, Context& ctx) : info(ctx.get(id)) {}
        explicit log_rate(Info& i) : info(i) {}

        template<class StreamCtx>
        decltype(auto) self_forward(StreamCtx&& ctx) {
            if (!info.check()) {
                ctx.context_valid = false;
            }
            return std::forward<StreamCtx>(ctx);
        }
    };

    /**
     * @brief 调用栈捕获修饰器 (基于 C++23/C++26 std::stacktrace)
     */
    struct log_stacktrace {
        usize skip_depth{2};
        bool auto_newline{true};

        explicit constexpr log_stacktrace(usize skip = 2, bool newline = true) noexcept
            : skip_depth(skip), auto_newline(newline) {}

        void write_to_log(pmr::string& str) const;
    };

    /// @brief 格式化标记 (持久)
    struct log_fmt {
        std::string_view fmt_str;
        explicit constexpr log_fmt(std::string_view s = "") noexcept : fmt_str(s) {}
    };

    /// @brief 临时单次格式化标记
    struct log_tfmt {
        std::string_view fmt_str;
        explicit constexpr log_tfmt(std::string_view s = "") noexcept : fmt_str(s) {}
    };
    using tfmt = log_tfmt;

    /// @brief 单条日志动态配置修饰器
    struct log_header {
        bool val{true};
        explicit constexpr log_header(bool v = true) noexcept : val(v) {}
        void manipulate(LogMsgConfig& cfg) const noexcept { cfg.out_header = val; }
    };

    struct log_level {
        bool val{true};
        explicit constexpr log_level(bool v = true) noexcept : val(v) {}
        void manipulate(LogMsgConfig& cfg) const noexcept { cfg.out_level = val; }
    };

    struct log_date {
        bool val{true};
        explicit constexpr log_date(bool v = true) noexcept : val(v) {}
        void manipulate(LogMsgConfig& cfg) const noexcept { cfg.gen_date = val; }
    };

    struct log_time {
        bool val{true};
        explicit constexpr log_time(bool v = true) noexcept : val(v) {}
        void manipulate(LogMsgConfig& cfg) const noexcept { cfg.gen_time = val; }
    };

    struct log_tid {
        bool val{true};
        explicit constexpr log_tid(bool v = true) noexcept : val(v) {}
        void manipulate(LogMsgConfig& cfg) const noexcept { cfg.gen_thread_id = val; }
    };

    /// @brief 忽略标记 (编译期空操作)
    struct log_nop {};

    /// @brief 擦除前置字符修饰器
    struct log_erase {
        usize count{0};
        explicit constexpr log_erase(usize c) noexcept : count(c) {}
    };

    /// @brief 自定义 Tag 注入修饰器
    struct log_tag {
        u16 category{0};
        u64 payload{0};

        constexpr log_tag(u16 cate_id, u64 p) noexcept
            : category(cate_id), payload(p) {}
    };

} // namespace alib6::log
