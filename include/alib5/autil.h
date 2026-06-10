/**
 * @file autil.h
 * @brief Utility library providing practical functions. / 工具库，提供实用函数。
 * @author aaaa0ggmc
 * @date 2026/06/10
 * @version 5.0
 * @copyright Copyright(c) 2026
 */

#ifndef ALIB5_AUTIL
#define ALIB5_AUTIL

#include <cctype>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <memory_resource>
#include <ranges>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <sys/types.h>

#include <alib5/detail/abase.h>
#include <alib5/detail/aerr_id.h>

#ifdef ALIB5_USE_VULKAN_H
#include <vulkan/vulkan.h>
#endif

#ifndef ALIB5_NOEXCEPT
#ifdef ALIB5_PANIC_USE_EXCEPTION
#define ALIB5_NOEXCEPT
#else 
#define ALIB5_NOEXCEPT noexcept
#endif
#endif

/**
 * @brief Specifies the default memory resource used by alib5.
 * @par Original Comment:
 * 虽然其实这个没啥用，但是这个还是指定了默认情况下alib5使用的内存资源
 */
#define ALIB5_DEFAULT_MEMORY_RESOURCE std::pmr::get_default_resource()

#define CAT(a, b) a##b
#define CAT_2(a, b) CAT(a, b)

/**
 * @brief Defer macro similar to Go's defer statement.
 */
#define $defer alib5::defer_t CAT_2(defer, __COUNTER__) = [&] ALIB5_NOEXCEPT  

/**
 * @brief Macro to conditionally invoke error handling based on the logging level.
 */
#define MAY_INVOKE(N) if constexpr(alib5::conf_lib_error_invoke_level >= (N))

/**
 * @brief Attempts to assign values between a range/view and other types.
 * 
 * @par Original Comment:
 * 提供对view和其他类型之间的尝试性赋值
 * 
 * @tparam T The target type.
 * @tparam R The range type.
 * @param t Target object.
 * @param r Range object.
 * @return Reference to the target object.
 */
template<class T, std::ranges::range R> 
inline T& operator|=(T& t, R&& r) {
    if constexpr (requires { t.assign(std::ranges::begin(r), std::ranges::end(r)); }) {
        t.assign(std::ranges::begin(r), std::ranges::end(r));
    } else {
        t = T(std::ranges::begin(r), std::ranges::end(r));
    }
    return t;
}

namespace alib5 {

    /**
     * @brief Default reserved size for pushing error strings.
     * @par Original Comment:
     * 默认的错误推送预留大小
     */
    constexpr uint32_t conf_error_string_reserve_size = 64;

    /**
     * @brief Default generic string reserved size.
     * @details It is not recommended to modify this if you are a library bin user, as some functions in the bin use the compiled size.
     * @par Original Comment:
     * 默认的通用预留大小（如果你是库bin版本使用者不建议修改，因为bin中部分函数用的是我编译时的大小）
     */
    constexpr uint32_t conf_generic_string_reserve_size = 128;

    /**
     * @brief Error invocation level for the library.
     * @details 
     * Level 0 : No output.
     * Level 1 : Output on fatal errors.
     * Level 2 : Output on minor errors.
     * Level 3 : Output everything.
     * @par Original Comment:
     * Level 0 : 没有输出
     * Level 1 : 致命错误的时候输出
     * Level 2 : 轻伤输出
     * Level 3 : 输出一切
     */
    constexpr uint32_t conf_lib_error_invoke_level = 1;

    /**
     * @brief Cursors for `misc::normalize_elapse`.
     * @details It is not recommended to modify this, otherwise it will cause function crashes.
     * @par Original Comment:
     * msic::normalize_elapse的游标
     * 不建议修改这个,不然会导致函数运行崩溃
     */
    constexpr std::array<std::string_view, 4> normalize_elapse_movers = {"s", "ms", "us", "ns"};
    constexpr size_t normalize_elapse_ms_index = 1;

    /**
     * @brief Used to identify log termination.
     * @details Since this completely lacks data, it can be used for cross-library interaction.
     * @par Original Comment:
     * 仅用来识别日志终止
     * 因为这个玩意儿完全不携带数据，因此可以用于多库交互
     */
    struct ALIB5_API LogEnd {};
    typedef void (*EndLogFn)(LogEnd);

    /**
     * @brief An alternative way to end logs.
     * @par Original Comment:
     * 另一种方式
     */
    constexpr LogEnd fls = {};

    /**
     * @brief Concept to determine if the input can be classified as string-like.
     * @par Original Comment:
     * 判断输入内容是否可以归类于string
     */
    template<class T> 
    concept IsStringLike = std::convertible_to<T, std::string_view>;

    using String = std::pmr::string;

    /**
     * @brief Concept to determine if a type supports string-like expansion functions.
     * @par Original Comment:
     * 判断是否支持字符串的扩充函数
     */
    template<class T> 
    concept CanExtendString = requires(T& t) {
        { t.data() } -> std::convertible_to<char*>;
        t.resize(static_cast<size_t>(1));
        { t.size() } -> std::convertible_to<size_t>;
        typename T::value_type;
    } && std::ranges::contiguous_range<T>;

    /**
     * @brief Enum representing severity levels, used as the default numerical mapping in the logger.
     * @par Original Comment:
     * 作为级别填充，同时也是Logger中默认的数值映射
     */
    enum class Severity : int {
        Trace = 0, ///< Trace level, basically holds no business significance. / Trace级别，基本上没什么业务意义
        Debug = 1, ///< Debug level, generally used in a debug environment. / Debug级别，一般用在Debug环境下
        Info  = 2, ///< Info level, standard log information. / Info 级别，普通日志信息
        Warn  = 3, ///< Warn level, a minor problem occurred. / Warn 级别，出了点小问题
        Error = 4, ///< Error level, a larger but acceptable problem. / Error级别，问题大了点，能接受
        Fatal = 5  ///< Fatal level, an unacceptable critical problem. / Fatal级别，不能接受的超级大问题
    };

    /**
     * @brief Gets the severity string used by alib.
     * @par Original Comment:
     * 获取alib使用的severity的字符串
     * @param sv Severity level.
     * @return String representation of the severity.
     */
    std::string_view get_severity_str(Severity sv);

    namespace detail {
        /**
         * @brief Error context information.
         * @details Designed for implicit construction to make handling easier.
         * @par Original Comment:
         * 错误信息
         * 用于触发implicit调用方便处理
         */
        struct ALIB5_API ErrorCtx {
            int64_t id;                 ///< Error ID / 错误的Id
            std::source_location loc;   ///< Location where the error occurred / 错误发生地点
            
            /**
             * @brief Implicit constructor taking an ID.
             * @par Original Comment:
             * 你只需要传递int64_t id通过implicit constructor生成对象
             */
            ErrorCtx(int64_t iid, std::source_location isl = std::source_location::current()) 
                : id(iid), loc(isl) {}
        };
        
        /**
         * @brief Transparent string hashing.
         * @details Avoids the need to build temporary pmr strings every time.
         * @par Original Comment:
         * 透明哈希处理,不然每次都需要构建临时pmr
         */
        struct TransparentStringHash {
            using is_transparent = void;

            size_t operator()(std::string_view sv) const ALIB5_NOEXCEPT {
                return std::hash<std::string_view>{}(sv);
            }
        };

        /**
         * @brief Transparent string equality checker.
         */
        struct TransparentStringEqual {
            using is_transparent = void;

            bool operator()(std::string_view lhs, std::string_view rhs) const ALIB5_NOEXCEPT {
                return lhs == rhs;
            }
        };
    }

    /**
     * @brief Runtime error structure generated by invoke_error.
     * @par Original Comment:
     * invoke_error产生的错误的结构
     */
    struct ALIB5_API RuntimeError {
        int64_t id;                 ///< Error ID / 错误id
        std::string_view data;      ///< Detailed error content / 错误具体的内容
        Severity severity;          ///< Severity of the error / 错误的严重程度
        std::source_location location; ///< Location of the error, generally for debugging / 一般调试用，错误出现的位置

        /**
         * @brief Format and write to a logger context.
         * @par Original Comment:
         * 兼容logger
         */
        template<class Data> 
        inline void write_to_log(Data& ctx) const {
            std::format_to(std::back_inserter(ctx), "[ID:{}][{}] {} in [{}:{}:{}]", 
                id,
                get_severity_str(severity),
                data,
                location.file_name(),
                location.line(),
                location.column()
            );
        }
    };   

    /// @brief Fast trigger function type for error handling. / 对于错误处理的快trigger函数
    typedef void(*RuntimeErrorTriggerFn)(const RuntimeError&);
    
    /// @brief Slow but comprehensive trigger function type for error handling. Fast processing is generally recommended. / 对于错误处理的慢但是全能的trigger函数，一般只建议使用快处理
    using RTErrorTriggerFnpp = std::function<void(const RuntimeError&)>;

    /**
     * @brief Invokes a new error.
     * @par Original Comment:
     * 发起新的错误
     */
    void ALIB5_API invoke_error(detail::ErrorCtx ctx, std::string_view data = "", Severity = Severity::Error) ALIB5_NOEXCEPT;
    
    /**
     * @brief Invokes a new error formatted using `std::format`.
     * @par Original Comment:
     * 使用fmt发起新的错误
     */
    template<class... Args>
    void invoke_error(detail::ErrorCtx ctx, std::string_view fmt, Severity, Args&&... args) ALIB5_NOEXCEPT;
    
    /**
     * @brief Invokes a new formatted error, defaulting to `Severity::Error`.
     * @par Original Comment:
     * 使用fmt发起新的错误，默认severity为error
     */
    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx, std::string_view fmt, Args&&... args) ALIB5_NOEXCEPT;

    /**
     * @brief Sets the fast error callback.
     * @par Original Comment:
     * 设置快触发函数
     */
    void ALIB5_API set_fast_error_callback(RuntimeErrorTriggerFn fn) ALIB5_NOEXCEPT;
    
    /**
     * @brief Adds a slow/heavy error callback.
     * @par Original Comment:
     * 设置慢触发函数
     */
    void ALIB5_API add_error_callback(RTErrorTriggerFnpp heavy_fn) ALIB5_NOEXCEPT;

    /**
     * @brief Defer utility similar to Go's defer processing.
     * @par Original Comment:
     * 类似Go语言的退出处理
     */
    template<class T> 
    struct defer_t {
        T defer_func;
        inline defer_t(T functor) : defer_func(std::move(functor)) {}
        inline ~defer_t() ALIB5_NOEXCEPT { defer_func(); }

        defer_t(const defer_t&) = delete;
        defer_t(defer_t&&) = delete;
        defer_t& operator=(const defer_t&) = delete;
        defer_t& operator=(defer_t&&) = delete;
    };

    namespace ext {
        /**
         * @brief Thread-local string buffer for formatted data.
         * @par Original Comment:
         * 缓存数据的字符串缓存
         */
        static inline thread_local String fmt_buf(ALIB5_DEFAULT_MEMORY_RESOURCE); 

        /**
         * @brief Converts to string (Specialized Path).
         * @par Original Comment:
         * 转换为string(特化路径)
         * @tparam copy If true, returns `std::string` which can be used to construct objects directly. If false, returns `std::string_view` which holds the internal buffer data. / 为true时返回std::string可用于直接构造对象，为false时返回std::string_view可以持有内部buffer的数据
         */
        template<bool copy = true, IsStringLike T> 
        [[nodiscard]] auto to_string(T&& v) ALIB5_NOEXCEPT;
        
        /**
         * @brief Converts to string (General).
         * @par Original Comment:
         * 转换为string(General)
         * @tparam copy If true, returns `std::string`. If false, returns `std::string_view`. / 为true时返回std::string可用于直接构造对象，为false时返回std::string_view可以持有内部buffer的数据
         */
        template<bool copy = true, class T> 
        [[nodiscard]] auto to_string(T&& v) ALIB5_NOEXCEPT;

        /**
         * @brief Converts string_view to other types.
         * @par Original Comment:
         * 转换为其他类型
         */
        template<class T> 
        auto to_T(std::string_view v, std::from_chars_result* result = nullptr) ALIB5_NOEXCEPT;
    
        template<bool copy = true, class... Args> 
        auto _to_string(std::string_view fmt, Args&&... args);
    }

    /// @brief Miscellaneous utility functions. / 杂类函数
    namespace misc {
        /**
         * @brief Gets the current formatted time.
         * @par Original Comment:
         * 返回格式化的当前时间
         */
        std::string_view ALIB5_API get_time() ALIB5_NOEXCEPT;
        
        /**
         * @brief Formats a duration in seconds.
         * @par Original Comment:
         * 格式化时间
         */
        std::string_view ALIB5_API format_duration(int seconds) ALIB5_NOEXCEPT;
        
        /**
         * @brief Detailed processing to normalize elapsed time.
         * @par Original Comment:
         * 对耗时进行细致化的处理
         */
        std::pair<double, std::string_view> ALIB5_API normalize_elapse(double elapse_ms);
    
        /**
         * @brief Defer Manager, similar to C++'s atexit approach.
         * @par Original Comment:
         * DeferManager,类似c++ atexit的方式
         */
        struct DeferManager {
        private:
            /**
             * @brief No need for pmr here, deemed unnecessary.
             * @par Original Comment:
             * 这个就不pmr了,感觉没啥必要
             */
            std::deque<std::move_only_function<void(void)>> defer_mgr;
        public:
            void defer(std::move_only_function<void(void)> function) {
                defer_mgr.emplace_back(std::move(function));
            }

            ~DeferManager() {
                while(!defer_mgr.empty()) {
                    auto fn = std::move(defer_mgr.back());
                    defer_mgr.pop_back();

                    if(fn) {
                        try { 
                            fn(); 
                        } catch(...) { 
                            // Log or ignore, exceptions must never escape the destructor.
                            // 记录日志或忽略，绝不能让异常逃出析构函数
                        }
                    }
                }
            }
        };
    }

    /// @brief String manipulation functions. / 字符串处理函数
    namespace str {
        /**
         * @brief Advanced removal of escape sequences.
         * @par Original Comment:
         * 更加高级的去除转义语序
         */
        std::string_view ALIB5_API unescape(std::string_view in) ALIB5_NOEXCEPT;
        
        /**
         * @brief Reverse process, applies escape sequences to a string.
         * @par Original Comment:
         * 反向处理，对字符串进行转义化
         */
        std::string_view ALIB5_API escape(std::string_view in, bool ensure_ascii = false) ALIB5_NOEXCEPT;
        
        /**
         * @brief Trims whitespace. Returns a substring view of the input.
         * @par Original Comment:
         * 去除空白字符，返回的是input的sub string_view
         */
        std::string_view ALIB5_API trim(std::string_view input);
        
        /**
         * @brief Splits a string by a string separator. Can recognize the entire string.
         * @par Original Comment:
         * 分割字符串，可以识别整个字符串,vector中为对source的切片
         */
        std::vector<std::string_view> ALIB5_API split(std::string_view source, std::string_view seps) ALIB5_NOEXCEPT;
        
        /**
         * @brief Splits a string by a character separator.
         * @par Original Comment:
         * 分割字符串,vector中为对source的切片
         */
        inline std::vector<std::string_view> ALIB5_API split(std::string_view source, const char sep) ALIB5_NOEXCEPT {
            return str::split(source, std::string_view(&sep, 1));
        }

        /**
         * @brief Converts string to uppercase.
         * @par Original Comment:
         * 大小写转换
         */
        inline auto to_upper(std::string_view input) {
            return input | std::views::transform([](const char& ch) {
                return std::toupper(ch);
            });
        }

        /**
         * @brief Converts string to lowercase.
         */
        inline auto to_lower(std::string_view input) {
            return input | std::views::transform([](const char& ch) {
                return std::tolower(ch);
            });
        }

        /**
         * @brief Simple string constant pool.
         * @par Original Comment:
         * 简易的字符串常量池
         */
        template<IsStringLike T>
        struct StringPool {
            std::pmr::unordered_set<
                T,
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > pool;

            StringPool(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
                : pool(__a) {}

            inline std::string_view get(std::string_view input) requires
            requires(std::string_view str) {
                T(str.begin(), str.end());
            } {
                auto it = pool.find(input);
                if(it != pool.end()) {
                    return *it;
                }
                return *pool.emplace(T(input.begin(), input.end())).first;
            }
        }; 
    }

    /// @brief File IO functions. / 文件io函数
    namespace io {
        /**
         * @brief Reads entire file contents into output.
         * @par Original Comment:
         * 读取文件
         */
        template<CanExtendString T> 
        size_t read_all(std::string_view path, T& output);
        
        /**
         * @brief Writes data to a file.
         * @par Original Comment:
         * 写入文件
         */
        template<CanExtendString T> 
        size_t write_all(std::string_view path, T& input);
        
        /**
         * @brief Holds file type and file name information.
         * @details For directories (including links), guarantees that it ends with "/" or "\\" (Windows).
         * @par Original Comment:
         * 保存了文件类型以及文件的名字
         * 其中，对于directory（包括link），保证末尾是"/"或者"\\"(Windows)
         */
        struct ALIB5_API FileEntry {
            /**
             * @brief File type extracted from `<filesystem>`.
             * @par Original Comment:
             * 文件类型，摘自<filesystem>
             */
            enum Type {
                none = 0,      not_found = -1, 
                regular = 1,   directory = 2, 
                symlink = 3,   block = 4,
                character = 5, fifo = 6,
                socket = 7,    unknown = 8
            };

            /// Move assignment operator / 移动函数
            inline void operator=(FileEntry&& entry) {
                path = std::move(entry.path);
                type = entry.type;
                subs = std::move(entry.subs);
                scanned = entry.scanned;
                last_write = entry.last_write;
            }
            
            /// Copy assignment operator / 复制函数
            inline void operator=(const FileEntry& entry) {
                path = entry.path;
                type = entry.type;
                subs = entry.subs;
                scanned = entry.scanned;
                last_write = entry.last_write;
            }
            
            inline FileEntry(FileEntry&& entry) ALIB5_NOEXCEPT {
                operator=(std::forward<FileEntry>(entry));
            }
            
            inline FileEntry(const FileEntry& entry) ALIB5_NOEXCEPT {
                *this = entry;
            }
            
            inline FileEntry() ALIB5_NOEXCEPT {}

            /**
             * @brief Checks if the entry is invalid.
             * @par Original Comment:
             * 是否有效
             */
            inline bool invalid() const {
                return (type == not_found) || (!path.compare(""));
            }

            /**
             * @brief Checks if the entry is a directory.
             * @par Original Comment:
             * 是否为文件夹
             */
            inline bool is_directory() const {
                return std::filesystem::is_directory(path);
            }

            /**
             * @brief Rescans the entry, applicable for manual scenarios.
             * @par Original Comment:
             * 重新扫描，适用于手动情况
             */
            inline void rescan() const {
                scanned = false;
                scan_subs();
            }

            inline size_t file_size() const {
                std::error_code ec;
                size_t size = std::filesystem::file_size(path, ec);
                if(ec) {
                    return std::variant_npos;
                }
                return size;
            }

            inline size_t elements_size() const {
                scan_subs();
                return subs.size();
            }

            inline auto begin() const { scan_subs(); return subs.cbegin(); }
            inline auto end() const { scan_subs(); return subs.cend(); }
            inline auto begin() { scan_subs(); return subs.begin(); }
            inline auto end() { scan_subs(); return subs.end(); }

            /**
             * @brief Gets a sub-directory or file entry.
             * @par Original Comment:
             * 获取子目录
             */
            const FileEntry& operator[](std::string_view str) const;
            
            /**
             * @brief Gets a sub-directory or file entry without requiring immediate scanning.
             * @par Original Comment:
             * 获取子目录，但是不需要进行立即扫描
             */
            const FileEntry& operator()(std::string_view str) const;

            /**
             * @brief Reads data from the file.
             * @par Original Comment:
             * 读取数据
             */
            template<CanExtendString T> 
            size_t read(T& output, std::size_t read_count = 0) const;
            
            /**
             * @brief Simpler version of reading data, returns a fixed pmr string.
             * @par Original Comment:
             * 读取数据，比较简单的版本，固定返回pmr
             */
            inline std::pmr::string read(std::size_t read_count = 0) const {
                std::pmr::string str(ALIB5_DEFAULT_MEMORY_RESOURCE);
                read(str, read_count);
                return str;
            }
            
            /**
             * @brief Writes data into the file.
             * @par Original Comment:
             * 写入数据
             */
            template<CanExtendString T> 
            size_t write_all(T& output) const;

            std::pmr::string path {ALIB5_DEFAULT_MEMORY_RESOURCE}; ///< File path / 文件路径
            Type type {not_found};                                 ///< File type / 类型
            std::filesystem::file_time_type last_write {};         ///< Last modified time / 上次更新时间

        private:
            mutable std::pmr::unordered_map<std::pmr::string, FileEntry> subs {ALIB5_DEFAULT_MEMORY_RESOURCE}; ///< Cached sub-objects / 缓存的子对象
            mutable bool scanned { false }; ///< Tracks if scanning was performed / 缓存是否已经构建
            
            /**
             * @brief Used for handling flattened retrieval. Usually not very large.
             * @par Original Comment:
             * 用于处理扁平化获取。一般大小不会很大
             */
            mutable std::pmr::unordered_map<std::pmr::string, FileEntry> flat_subs {ALIB5_DEFAULT_MEMORY_RESOURCE};

            static const FileEntry& gen_invalid(); ///< Generates an invalid entry / 生成不有效的数据
            void scan_subs() const;                ///< Searches for subdirectories / 搜索子目录

            /**
             * @brief Checks if the entry has been updated.
             * @par Original Comment:
             * 查看是否更新
             */
            bool updated() {
                if(invalid()) return false;
                std::error_code ec;
                auto t = std::filesystem::last_write_time(path, ec);
                if(ec) {
                    invoke_error(err_filesystem_error, "Failed to check \"{}\" 's last write time!", path);
                    return true;
                }
                if(last_write != t) {
                    last_write = t;
                    return true;
                }
                return false;
            }
        };

        FileEntry ALIB5_API load_entry(std::string_view, bool force_existence = true) ALIB5_NOEXCEPT;

        /**
         * @brief Results of directory traversal.
         * @par Original Comment:
         * 遍历目录的结果
         */
        struct ALIB5_API TraverseData {
            /**
             * @brief Absolute path of the traversed root directory.
             * @details Always ends with "/" or "\\" (ALIB_PATH_SEP).
             * @par Original Comment:
             * 遍历的根目录的绝对路径
             * 对于root_absolute，必定以"/"或者"\" (为ALIB_PATH_SEPS)结尾，比如'C:\','/'
             */
            std::pmr::string root_absolute;
            
            /**
             * @brief Absolute paths of traversed results.
             * @par Original Comment:
             * 遍历的结果的绝对路径
             */
            std::pmr::vector<FileEntry> targets_absolute;

            /**
             * @brief Retrieves relative path for a specific entry based on `root_absolute`.
             * @details You must guarantee that the entry comes from this traversed data itself.
             * @par Original Comment:
             * 获取entry中对应的相对路径，相对于root
             * 这里你需要自己保证entry的来源是traverse data本身
             * 获取relative的方式为对path进行substr从而裁剪出相对路径
             */
            inline std::string_view try_get_relative(FileEntry& entry) {
                return std::string_view(entry.path).substr(root_absolute.size());
            }

            using cont = std::pmr::vector<FileEntry>; ///< Overrides std usage scenarios / 覆盖std使用场景
            using iterator = cont::iterator;
            
            inline auto begin() { return targets_absolute.begin(); }
            inline auto end() { return targets_absolute.end(); }
            inline auto begin() const { return targets_absolute.cbegin(); }
            inline auto end() const { return targets_absolute.cend(); }
        };

        /**
         * @brief Configuration for traversing files.
         * @par Original Comment:
         * 遍历文件的配置
         */
        struct ALIB5_API TraverseConfig {
            /**
             * @brief Traversal depth. Values < 0 traverse everything.
             * @par Original Comment:
             * 遍历深度，小于0表示遍历所有
             */
            int depth { -1 }; 
            
            /**
             * @brief Ignore logic evaluated against the relative path.
             * @par Original Comment:
             * 基于遍历根目录，比如 traverse /home   --> "aaaa0ggmc" "aaaa0ggmc/..."
             */
            std::vector<std::function<bool(std::string_view)>> ignore;
            
            /**
             * @brief Types to retain.
             * @par Original Comment:
             * 保留哪些类型
             */
            std::vector<FileEntry::Type> keep { FileEntry::regular };

        private:
            friend TraverseData ALIB5_API traverse_files(std::string_view, TraverseConfig) ALIB5_NOEXCEPT;
            
            /**
             * @brief Builds the allow array.
             * @par Original Comment:
             * 构建allow数组
             */
            void build() ALIB5_NOEXCEPT;
            
            /**
             * @brief Simplified processing to increase performance.
             * @par Original Comment:
             * 对于keep进行简易处理增加性能
             */
            bool allow[10] {false};
        };

        /**
         * @brief Traverses files according to configuration.
         * @param file_path Supports relative and absolute paths. / 支持相对路径和绝对路径
         * @par Original Comment:
         * 遍历文件
         */
        TraverseData ALIB5_API traverse_files(std::string_view file_path, TraverseConfig config = TraverseConfig()) ALIB5_NOEXCEPT;
    }

    namespace sys {
    #ifdef _WIN32
        /**
         * @brief Enables the virtual terminal in Windows.
         * @details Necessary for colored escape sequence output.
         * @par Original Comment:
         * 启用系统的虚拟控制台，这样才能进行彩色转义输出
         */
        ALIB5_API void enable_virtual_terminal() ALIB5_NOEXCEPT;
    #else
        /**
         * @brief Enables the virtual terminal. (Usually a no-op on non-Windows).
         * @par Original Comment:
         * 启用系统虚拟控制台（基本上都是支持的）
         */
        inline void enable_virtual_terminal() ALIB5_NOEXCEPT {}
    #endif
        
        /**
         * @brief Retrieves the brand name of the system's CPU.
         * @par Original Comment:
         * 获取系统的CPU的品牌名
         */
        std::string_view ALIB5_API get_cpu_brand() ALIB5_NOEXCEPT;

        /**
         * @brief Structure mapping memory usage details. Unit is bytes.
         * @par Original Comment:
         * 内存占用结构体,单位byte
         */
        struct ALIB5_API ProgramMemUsage {
            mem_bytes memory;       ///< Memory usage / 内存占用
            mem_bytes virt_memory;  ///< Virtual memory usage / 虚拟内存占用
        };

        /**
         * @brief Structure mapping global memory usage. Unit is bytes.
         * @par Original Comment:
         * 全局内存占用结构体,单位byte
         */
        struct ALIB5_API GlobalMemUsage {
            /**
             * @brief Occupancy percentage.
             * @attention On Linux, it equals `physicalUsed / physicalTotal`. On Windows, it uses `dwMemoryLoad` from the API.
             * @par Original Comment:
             * 占用百分比,详情有注意事项 @attention Linux上等于 physicalUsed/physicalTotal,Windows上使用API提供的dwMemoryLoad
             */
            unsigned int percent;     
            mem_bytes physical_total; ///< Total physical memory / 物理内存总值
            mem_bytes virtual_total;  ///< Total virtual memory. Fixed to 0 on Linux temporarily. / 虚拟内存总值，Linux上暂时固定为0
            mem_bytes physical_used;  ///< Used physical memory / 物理内存占用值
            mem_bytes virtual_used;   ///< Used virtual memory. Fixed to 0 on Linux temporarily. / 虚拟内存占用值，Linux上暂时固定为0
            mem_bytes page_total;     ///< Total paging file size. Serves as SwapSize on Linux. / 页面文件总值，Linux上为SwapSize
            mem_bytes page_used;      ///< Used paging file size. Serves as SwapUsed on Linux. / 页面文件占用值,Linux上为SwapUsed
        };

        /**
         * @brief Retrieves program memory usage.
         * @par Original Comment:
         * 获取程序内存占用
         */
        ProgramMemUsage ALIB5_API get_prog_mem_usage() ALIB5_NOEXCEPT;

        /**
         * @brief Retrieves system global memory usage.
         * @par Original Comment:
         * 获取系统内存占用
         */
        GlobalMemUsage ALIB5_API get_global_mem_usage() ALIB5_NOEXCEPT;
    }
}

// -----------------------------------------------------------------------------
// Unified inline implementations
// 统一的inline实现
// -----------------------------------------------------------------------------
namespace alib5 {

    template<CanExtendString T> 
    size_t io::write_all(std::string_view path, T& output) {
        if(path.empty()) return std::variant_npos;
        // Fearful that path is sliced from a string, not building a string could cause errors.
        // 这里主要是惧怕path是从一段字符串截取下来的，不构建string会导致输出错误
        FILE* f = std::fopen(std::string(path).c_str(), "wb");
        if(!f) {
            invoke_error(err_io_error, "Failed to open file {}!", path);
            return std::variant_npos;
        }
        auto sz = std::fwrite(output.data(), sizeof(typename T::value_type), output.size(), f);
        std::fclose(f);
        return sz;
    }

    template<CanExtendString T> 
    size_t io::FileEntry::write_all(T& input) const {
        return alib5::io::write_all(path, input);
    }

    template<CanExtendString T> 
    inline size_t io::FileEntry::read(T& output, std::size_t read_count) const {
        if(!read_count) read_count = file_size();
        if(read_count == std::variant_npos) {
            invoke_error(err_io_error, "File {} is invalid!", path);
            return std::variant_npos;
        }

        FILE* f = std::fopen(path.c_str(), "rb");
        if(f == NULL) {
            invoke_error(err_io_error, "Failed to open file {}!", path);
            return std::variant_npos;
        }

        size_t old_size = output.size();
        output.resize(old_size + read_count);
        read_count = std::fread(output.data() + old_size, sizeof(char), read_count, f);
        std::fclose(f);
        return read_count;
    }

    template<CanExtendString T> 
    inline size_t io::read_all(std::string_view path, T& output) {
        io::FileEntry entry = io::load_entry(path);
        auto size = entry.file_size();
        if(size == std::variant_npos) {
            invoke_error(err_io_error, "File {} is invalid!", path);
            return size;
        }
        return entry.read(output, size);
    }

    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx, std::string_view fmt, Severity sv, Args&&... args) ALIB5_NOEXCEPT {
        try {
            std::pmr::string data(ALIB5_DEFAULT_MEMORY_RESOURCE);
            data.reserve(conf_error_string_reserve_size);
            std::vformat_to(std::back_inserter(data), fmt, std::make_format_args(args...));
            invoke_error(ctx, data, sv);
        } catch(...) {
            invoke_error(ctx, fmt, sv);
        }
    }

    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx, std::string_view fmt, Args&&... args) ALIB5_NOEXCEPT {
        invoke_error(ctx, fmt, Severity::Error, std::forward<Args>(args)...);
    }

    namespace ext {
        template<bool copy, IsStringLike T> 
        [[nodiscard]] auto to_string(T&& v) ALIB5_NOEXCEPT {
            if constexpr(std::is_pointer_v<decltype(v)>) {
                [[unlikely]] if(!v) return "";
            }
            fmt_buf.clear();
            fmt_buf = v;
            if constexpr(copy) {
                return String(fmt_buf.c_str(), ALIB5_DEFAULT_MEMORY_RESOURCE);
            } else return std::string_view(fmt_buf);
        }

        template<bool copy, class... Args> 
        auto _to_string(std::string_view fmt, Args&&... args) {
            fmt_buf.clear();
            try {
                std::vformat_to(
                    std::back_inserter(fmt_buf),
                    fmt,
                    std::make_format_args(args...)
                );
            } catch(...) {
                fmt_buf = "[FORMAT_ERROR]";
                MAY_INVOKE(3) {
                    invoke_error(err_format_error, "Failed to format the target!");
                }
            }
            if constexpr(copy) {
                return String(fmt_buf.c_str(), ALIB5_DEFAULT_MEMORY_RESOURCE);
            } else return std::string_view(fmt_buf);
        }

        template<bool copy, class T> 
        [[nodiscard]] auto to_string(T&& v) ALIB5_NOEXCEPT {
            // Processing with std::string directly where possible is actually more efficient.
            // std::string能处理的给std::string,实际上效率会更高
            using PureT = std::decay_t<T>;
            if constexpr(std::is_arithmetic_v<PureT> || std::is_same_v<PureT, bool>) {
                fmt_buf = std::to_string(std::forward<T>(v));
            } else {
                fmt_buf.clear();
                try {
                    std::format_to(std::back_inserter(fmt_buf), "{}", std::forward<T>(v));
                } catch(...) {
                    fmt_buf = "[FOMRAT_ERROR]";
                    MAY_INVOKE(3) {
                        invoke_error(err_format_error, "Failed to format the target!");
                    }
                }
            }
            if constexpr(copy) {
                return String(fmt_buf.c_str(), ALIB5_DEFAULT_MEMORY_RESOURCE);
            } else return std::string_view(fmt_buf);
        }
    }

    inline std::string_view get_severity_str(Severity sv) {
        switch(sv) {
        case Severity::Trace: return "TRACE";
        case Severity::Debug: return "DEBUG";
        case Severity::Info:  return "INFO ";
        case Severity::Warn:  return "WARN ";
        case Severity::Error: return "ERROR";
        case Severity::Fatal: return "FATAL";
        default: return "?????";
        }
    }

    template<class T> 
    inline auto ext::to_T(std::string_view v, std::from_chars_result* iresult) ALIB5_NOEXCEPT {
        auto report = [](std::error_code ec) {
            if(ec) {
                invoke_error(err_format_error, ec.message());
            }
        };
    
        if constexpr(std::is_same_v<T, bool>) {
            // Adaptation for booleans. Avoiding simple lowercase conversion to avoid weird values like tRUe.
            // 适配一些形式,之所以不用转大小写是因为你不觉得 tRUe很诡异吗
            if(v == "true" || v == "1" || v == "TRUE" || v == "True") return true;
            if(v == "false" || v == "0" || v == "FALSE" || v == "False") return false;
            return false;
        } else if constexpr(std::is_arithmetic_v<T>) {
            T val = T();
            auto result = std::from_chars(v.data(), v.data() + v.size(), val);
            if(iresult) *iresult = result;
            else report(std::make_error_code(result.ec));
            return val;
        } else if constexpr(IsStringLike<T>) {
            return static_cast<std::string_view>(v);
        } else {
            static_assert(IsStringLike<T>, "Failed to convert the value!");
        }
    }
}

/// Vulkan and External handling / 处理Vulkan之类的
#ifdef ALIB5_USE_VULKAN_H
namespace alib5::ext {
    template<bool copy = true> 
    auto vulkan_api_to_string(uint32_t spec) {
        return _to_string<copy>("{}.{}.{}.{}", 
            VK_API_VERSION_VARIANT(spec),
            VK_API_VERSION_MAJOR(spec),
            VK_API_VERSION_MINOR(spec),
            VK_API_VERSION_PATCH(spec)
        );
    }
}
#endif

#endif // ALIB5_AUTIL