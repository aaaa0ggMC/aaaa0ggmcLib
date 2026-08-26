/**
 * @file targets.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 内置日志输出目标 (Console 控制台、File 文件、RotateFile 轮转文件与内存缓冲)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#define ALIB6_ISATTY _isatty
#define ALIB6_FILENO _fileno
#else
#include <unistd.h>
#define ALIB6_ISATTY isatty
#define ALIB6_FILENO fileno
#endif

export module alib6.log:targets;
import std;
import alib6.core;
import :config;
import :msg;
import :mod;
import :manip;

namespace pmr = std::pmr;

export namespace alib6::log {

    /// @brief 终端前景色与背景色枚举
    enum class Color : u8 {
        None    = 0,
        Black   = 30, Red     = 31, Green   = 32, Yellow  = 33,
        Blue    = 34, Magenta = 35, Cyan    = 36, Gray    = 37, White   = 37,
        LRed    = 91, LGreen  = 92, LYellow = 93, LBlue   = 94,
        LMagenta= 95, LCyan   = 96, LGray   = 90, LWhite  = 97
    };

    /// @brief 终端字体样式掩码
    enum class Style : u16 {
        None      = 0,
        Bold      = 1 << 0,
        Dim       = 1 << 1,
        Italic    = 1 << 2,
        Underline = 1 << 3,
        Blink     = 1 << 4,
        Reverse   = 1 << 5,
        Hidden    = 1 << 6
    };

    /**
     * @brief 构建终端彩色输出修饰标签
     */
    [[nodiscard]] constexpr log_tag color(
        Color fg,
        Color bg = Color::None,
        Style style = Style::None,
        u16 cate_id = 0
    ) noexcept {
        if (fg == Color::None && bg == Color::None && style == Style::None) {
            return log_tag(0, 0);
        }

        u8 fg_val = static_cast<u8>(fg);
        u8 bg_val = (bg == Color::None) ? 0 : (static_cast<u8>(bg) + 10);
        u64 payload = (static_cast<u64>(style) << 16) | (static_cast<u64>(bg_val) << 8) | fg_val;
        return log_tag(cate_id, payload);
    }

    /**
     * @brief 合并两个终端颜色/样式载荷 (支持样式叠加、前景色与背景色按需覆盖)
     */
    [[nodiscard]] constexpr u64 combine_color_payload(u64 base_payload, u64 new_payload) noexcept {
        if (new_payload == 0) return 0; // 重置为 None

        u8 new_fg = static_cast<u8>(new_payload & 0xFF);
        u8 new_bg = static_cast<u8>((new_payload >> 8) & 0xFF);
        u16 new_style = static_cast<u16>((new_payload >> 16) & 0xFFFF);

        u8 base_fg = static_cast<u8>(base_payload & 0xFF);
        u8 base_bg = static_cast<u8>((base_payload >> 8) & 0xFF);
        u16 base_style = static_cast<u16>((base_payload >> 16) & 0xFFFF);

        u8 final_fg = (new_fg != 0) ? new_fg : base_fg;
        u8 final_bg = (new_bg != 0) ? new_bg : base_bg;
        u16 final_style = base_style | new_style;

        return (static_cast<u64>(final_style) << 16) | (static_cast<u64>(final_bg) << 8) | final_fg;
    }

    /**
     * @brief 专门针对 Console 控制台颜色标签的线性合并器 (Console Color Tag Merger)
     */
    template<class Alloc>
    inline bool merge_color_tag(
        const LogCustomTag& incoming,
        std::vector<LogCustomTag, Alloc>& tags,
        usize& slot_begin,
        bool combine_style = true
    ) {
        usize in_pos = incoming.get_pos();
        while (slot_begin < tags.size() && tags[slot_begin].get_pos() < in_pos) {
            ++slot_begin;
        }

        for (usize i = slot_begin; i < tags.size() && tags[i].get_pos() == in_pos; ++i) {
            if (tags[i].category == incoming.category) {
                if (combine_style) {
                    tags[i].payload = combine_color_payload(tags[i].payload, incoming.payload);
                } else {
                    tags[i].payload = incoming.payload;
                }
                return true;
            }
        }

        tags.insert(tags.begin() + slot_begin, incoming);
        return true;
    }

    /**
     * @brief 专门针对 Console 控制台颜色标签的紧凑化与状态消抖预制菜 (Console Color Tag Compactor)
     */
    template<class Alloc>
    inline void compact_color_tags(
        std::vector<LogCustomTag, Alloc>& tags,
        u16 color_category_id = 0
    ) {
        if (tags.size() <= 1) return;

        std::stable_sort(tags.begin(), tags.end(), [](const LogCustomTag& a, const LogCustomTag& b) {
            return a.get_pos() < b.get_pos();
        });

        usize write_idx = 0;
        for (usize read_idx = 0; read_idx < tags.size(); ++read_idx) {
            const auto& cur = tags[read_idx];

            if (cur.category == color_category_id) {
                // 检查同一个 pos 是否有后续同为颜色类别的覆写 tag
                bool overridden = false;
                for (usize lookahead = read_idx + 1; lookahead < tags.size(); ++lookahead) {
                    if (tags[lookahead].get_pos() != cur.get_pos()) break;
                    if (tags[lookahead].category == color_category_id) {
                        overridden = true;
                        break;
                    }
                }
                if (overridden) continue;
            }

            tags[write_idx++] = cur;
        }
        tags.resize(write_idx);
    }

    /**
     * @brief 控制台输出配色方案配置
     */
    struct ConsoleConfig {
        using ColorSchemaFn = std::string_view (*)(const LogMsg& msg);

        static std::string_view default_level_color_schema(const LogMsg& msg) noexcept {
            switch (msg.level) {
                case 0: return "\033[90m";     // Trace -> 亮灰
                case 1: return "\033[36m";     // Debug -> 青色
                case 2: return "\033[32m";     // Info  -> 绿色
                case 3: return "\033[33m";     // Warn  -> 黄色
                case 4: return "\033[31m";     // Error -> 红色
                case 5: return "\033[1;37;41m";// Fatal -> 红底白字粗体
                default: return "";
            }
        }

        std::FILE* output_target{stdout};

        ColorSchemaFn head_color_schema{nullptr};
        ColorSchemaFn body_color_schema{nullptr};
        ColorSchemaFn time_color_schema{nullptr};
        ColorSchemaFn date_color_schema{nullptr};
        ColorSchemaFn thread_id_color_schema{nullptr};
        ColorSchemaFn level_color_schema{default_level_color_schema};
    };

    /**
     * @brief 控制台输出目标 (支持 ANSI 颜色、多线程写入锁保护与彩色标签高亮)
     */
    struct Console : public LogTarget {
        static inline std::mutex console_lock;

        ConsoleConfig cfg;
        std::FILE* out{stdout};
        bool support_colors{false};
        u16 category_id{0};

        explicit Console(u16 cat_id = 0, ConsoleConfig c = ConsoleConfig{})
            : cfg(c), out(c.output_target ? c.output_target : stdout), category_id(cat_id) {
            support_colors = (ALIB6_ISATTY(ALIB6_FILENO(out)) != 0);
        }

        void write(LogMsg& msg) override;

        void flush() override {
            std::lock_guard<std::mutex> lock(console_lock);
            if (out) std::fflush(out);
        }
    };

    /**
     * @brief 单文件持久化输出目标
     */
    struct File : public LogTarget {
        std::FILE* file{nullptr};
        pmr::string currently_open;
        bool need_close{true};

        void open_file(std::string_view fp) {
            if (file && need_close) {
                std::fclose(file);
                file = nullptr;
            }
            currently_open = fp;
            file = std::fopen(currently_open.c_str(), "a");
            panic_if(!file, "File::open_file cannot open target file!");
        }

        explicit File(std::string_view fpath, memory_resource* mem = get_default_resource())
            : currently_open(mem) {
            open_file(fpath);
        }

        explicit File(std::FILE* f) : file(f), need_close(false) {}

        void write(LogMsg& msg) override {
            if (!file) return;
            auto p = msg.gen_composed();
            std::fwrite(p.data(), 1, p.size(), file);
        }

        void flush() override {
            if (file) std::fflush(file);
        }

        void close() override {
            if (need_close && file) {
                std::fclose(file);
                file = nullptr;
            }
        }
    };

    struct RotateFile;

    /**
     * @brief 轮转文件配置
     */
    struct RotateFileConfig {
        using IOFailedCallbackFn = std::function<void(std::string_view, RotateFile&)>;
        using IOFailedCallbackFN = IOFailedCallbackFn;

        /// @brief 轮转文件路径格式 (支持 {0}: 当前时间戳, {1}: 轮转序号)
        std::string_view filepath_fmt{"log_{1}.txt"};
        /// @brief 单文件最大容量字节数 (默认 4MB，0 表示不基于大小轮转)
        u64 rotate_size{4 * 1024 * 1024};
        /// @brief 轮转最大时间间隔毫秒数 (-1 表示不基于时间轮转)
        i64 rotate_time_ms{-1};
        /// @brief 打开文件失败时的回调函数
        IOFailedCallbackFn failed_open_fn{nullptr};

        RotateFileConfig() = default;
        RotateFileConfig(
            std::string_view fmt,
            u64 ro_size = 4 * 1024 * 1024,
            i64 ro_time = -1,
            IOFailedCallbackFn fn = nullptr
        ) : filepath_fmt(fmt)
          , rotate_size(ro_size)
          , rotate_time_ms(ro_time)
          , failed_open_fn(std::move(fn)) {}
    };

    /**
     * @brief 自动按大小或时间滚动的轮转文件输出目标
     */
    struct RotateFile : public LogTarget {
    private:
        std::FILE* f{nullptr};
        int rotate_index{0};
        u64 bytes_written{0};
        pmr::string current_fp;
        RotateFileConfig config;
        std::chrono::steady_clock::time_point last_expire{};

        [[nodiscard]] bool expires() noexcept {
            auto now = std::chrono::steady_clock::now();
            bool size_exceeded = config.rotate_size > 0 && bytes_written >= config.rotate_size;
            if (size_exceeded) {
                last_expire = now;
                return true;
            }

            if (config.rotate_time_ms > 0) {
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_expire).count();
                if (elapsed_ms >= config.rotate_time_ms) {
                    last_expire = now;
                    return true;
                }
            }
            return false;
        }

    public:
        [[nodiscard]] std::string_view get_current_filepath() {
            current_fp.clear();
            auto cur_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::vformat_to(
                std::back_inserter(current_fp),
                config.filepath_fmt,
                std::make_format_args(cur_time, rotate_index)
            );
            return current_fp;
        }

        void try_open() {
            if (f && !expires()) return;
            close();
            auto path = get_current_filepath();
            f = std::fopen(path.data(), "a");
            if (!f && config.failed_open_fn) {
                config.failed_open_fn(path, *this);
            } else if (f) {
                ++rotate_index;
                bytes_written = 0;
            }
        }

        void flush() override {
            if (f) std::fflush(f);
        }

        void close() override {
            if (f) {
                std::fclose(f);
                f = nullptr;
                bytes_written = 0;
            }
        }

        void write(LogMsg& msg) override {
            try_open();
            if (f) {
                auto p = msg.gen_composed();
                bytes_written += std::fwrite(p.data(), 1, p.size(), f);
            }
        }

        explicit RotateFile(RotateFileConfig cfg = RotateFileConfig{}, memory_resource* mem = get_default_resource())
            : current_fp(mem), config(cfg), last_expire(std::chrono::steady_clock::now()) {
            try_open();
        }
    };

    /**
     * @brief 异步 Promise 字符串接收目标 (一次性写入)
     */
    template<class StrT = std::string>
    struct ConsoleBuffer : public LogTarget {
        std::mutex buffer_lock;
        ConsoleConfig cfg;
        u16 category_id{0};
        std::promise<StrT>* data{nullptr};

        ConsoleBuffer(std::promise<StrT>& d, u16 cat_id = 0, ConsoleConfig c = ConsoleConfig{})
            : cfg(c), category_id(cat_id), data(&d) {}

        void set(std::promise<StrT>& d) {
            std::lock_guard<std::mutex> lk(buffer_lock);
            data = &d;
        }

        void detach() {
            std::lock_guard<std::mutex> lk(buffer_lock);
            data = nullptr;
        }

        void write(LogMsg& msg) override;
    };

    /**
     * @brief 具备条件变量同步通知的动态字符串内存缓冲目标
     */
    template<class StrT = std::string>
    struct SyncConsoleBuffer : public LogTarget {
        std::mutex buffer_lock;
        ConsoleConfig cfg;
        u16 category_id{0};
        StrT data{};
        u64 version_code{0};
        std::condition_variable cv;

        explicit SyncConsoleBuffer(StrT d = StrT{}, u16 cat_id = 0, ConsoleConfig c = ConsoleConfig{})
            : cfg(c), category_id(cat_id), data(std::move(d)), version_code(0) {}

        [[nodiscard]] bool updated(u64 old_version) const noexcept {
            return version_code != old_version;
        }

        [[nodiscard]] u64 version() const noexcept {
            return version_code;
        }

        void write(LogMsg& msg) override;

        void wait_update(u64 old_version) {
            std::unique_lock<std::mutex> lk(buffer_lock);
            cv.wait(lk, [this, old_version] {
                return version_code != old_version;
            });
        }
    };

} // namespace alib6::log
