/**@file autil.h
* @brief 工具库，提供实用函数
* @author aaaa0ggmc
* @date 2026/02/04
* @version 5.0
* @copyright Copyright(c) 2026
*/
#ifndef ALIB5_AUTIL
#define ALIB5_AUTIL
#include <alib5/detail/abase.h>
#include <alib5/detail/aerr_id.h>
#include <cctype>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <ranges>
#include <system_error>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory_resource>
#include <source_location>
#include <string>
#include <functional>
#include <string_view>
#include <sys/types.h>
#include <format>
#include <type_traits>
#include <utility>
#include <cstdio>
#include <unordered_set>

/// 虽然其实这个没啥用，但是这个还是指定了默认情况下alib5使用的内存资源
#define ALIB5_DEFAULT_MEMORY_RESOURCE std::pmr::get_default_resource()
#define CAT(a,b) a##b
#define CAT_2(a,b) CAT(a,b)
#define defer alib5::defer_t CAT_2(defer,__COUNTER__) = [&] noexcept  
#define MAY_INVOKE(N) if constexpr(conf_lib_error_invoke_level >= (N))

/// 提供对view和其他类型之间的尝试性赋值
template<class T,std::ranges::range R> inline
T& operator|=(T & t,R && r){
    if constexpr (requires { t.assign(std::ranges::begin(r), std::ranges::end(r)); }) {
        t.assign(std::ranges::begin(r), std::ranges::end(r));
    } else {
        t = T(std::ranges::begin(r), std::ranges::end(r));
    }
    return t;
}

namespace alib5{
    /// 默认的错误推送预留大小
    constexpr uint32_t conf_error_string_reserve_size = 64;
    /// 默认的通用预留大小（如果你是库bin版本使用者不建议修改，因为bin中部分函数用的是我编译时的大小）
    constexpr uint32_t conf_generic_string_reserve_size = 128;
    /// Level 0 : 没有输出
    /// Level 1 : 致命错误的时候输出
    /// Level 2 : 轻伤输出
    /// Level 3 : 输出一切
    constexpr uint32_t conf_lib_error_invoke_level = 1;
    /// 判断输入内容是否可以归类于string
    template<class T> concept IsStringLike = std::convertible_to<T,std::string_view>;
    using String = std::pmr::string;
    /// 判断是否支持字符串的扩充函数
    template<class T> concept CanExtendString = requires(T & t){
        { t.data() } -> std::convertible_to<char*>;
        t.resize((size_t)1);
        { t.size() } -> std::convertible_to<size_t>;
        typename T::value_type;
    } && std::ranges::contiguous_range<T>;

    namespace detail{
        // 用于触发implicit调用方便处理
        /// 错误信息
        struct ALIB5_API ErrorCtx{
            int64_t id;                 ///< 错误的Id
            std::source_location loc;   ///< 错误发生地点
            
            /// 你只需要传递int64_t id通过implicit constructor生成对象
            ErrorCtx(int64_t iid,std::source_location isl = std::source_location::current()):id(iid),loc(isl){}
        };
        
        /// 透明哈希处理,不然每次都需要构建临时pmr
        struct TransparentStringHash {
            // 激活异构查找的关键
            using is_transparent = void;

            // 使用 std::hash<std::string_view> 作为基础，因为它能处理各种字符串容器
            size_t operator()(std::string_view sv) const noexcept {
                return std::hash<std::string_view>{}(sv);
            }
            /*
                size_t operator()(const std::pmr::string& s) const noexcept {
                    return std::hash<std::string_view>{}(s);
                }
                
                size_t operator()(const std::string& s) const noexcept {
                    return std::hash<std::string_view>{}(s);
            }
            */
        };

        struct TransparentStringEqual {
            using is_transparent = void;

            // 利用 string_view 的 operator==，它已经重载了各种字符串组合
            bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
                return lhs == rhs;
            }
        };
    }

    namespace ext{
        /// 缓存数据的字符串缓存
        static inline thread_local String fmt_buf(ALIB5_DEFAULT_MEMORY_RESOURCE); 

        /// 转换为string(特化路径)
        /// @tparam copy 为true时返回std::string可用于直接构造对象，为false时返回std::string_view可以持有内部buffer的数据
        template<IsStringLike T,bool copy = true> [[nodiscard]] auto to_string(T && v) noexcept;
        /// 转换为string(General)
        /// @tparam copy 为true时返回std::string可用于直接构造对象，为false时返回std::string_view可以持有内部buffer的数据
        template<class T,bool copy = true> [[nodiscard]] auto to_string(T && v) noexcept;

        /// 转换为其他类型
        template<class T> auto to_T(std::string_view v,std::from_chars_result * result = nullptr) noexcept;
    };

    /// @brief 类似Go语言的退出处理
    template<class T> struct defer_t{
        T defer_func;
        inline defer_t(T functor):defer_func(std::move(functor)){}
        inline ~defer_t() noexcept{defer_func();}

        defer_t(const defer_t &) = delete;
        defer_t(defer_t &&) = delete;
        defer_t& operator=(const defer_t&) = delete;
        defer_t& operator=(defer_t&&) = delete;
    };

    /// 作为级别填充，同时也是Logger中默认的数值映射
    enum class Severity : int{
        Trace = 0, ///< Trace级别，基本上没什么业务意义
        Debug = 1, ///< Debug级别，一般用在Debug环境下
        Info  = 2, ///< Info 级别，普通日志信息
        Warn  = 3, ///< Warn 级别，出了点小问题
        Error = 4, ///< Error级别，问题大了点，能接受
        Fatal = 5  ///< Fatal级别，不能接受的超级大问题
    };
    /// 获取alib使用的severity的字符串
    std::string_view get_severity_str(Severity sv);

    /// invoke_error产生的错误的结构
    struct ALIB5_API RuntimeError{
        /// 错误id
        int64_t id;
        /// 错误具体的内容
        std::string_view data;
        /// 错误的严重程度
        Severity severity;
        /// 一般调试用，错误出现的位置
        std::source_location location;

        /// 兼容logger
        template<class Data> inline void write_to_log(Data & ctx) const{
            std::format_to(std::back_inserter(ctx),"[ID:{}][{}] {} in [{}:{}:{}]", 
                id,
                get_severity_str(severity),
                data,
                location.file_name(),
                location.line(),
                location.column()
            );
        }
    };   

    /// 对于错误处理的快trigger函数
    typedef void(*RuntimeErrorTriggerFn)(const RuntimeError & );
    /// 对于错误处理的慢但是全能的trigger函数，一般只建议使用快处理
    using RTErrorTriggerFnpp = std::function<void(const RuntimeError &)>;

    /// 发起新的错误
    void ALIB5_API invoke_error(detail::ErrorCtx ctx,std::string_view data = "",Severity = Severity::Error) noexcept;
    /// 使用fmt发起新的错误
    template<class... Args>
    void invoke_error(detail::ErrorCtx ctx,std::string_view fmt,Severity,Args&&... args) noexcept;
    /// 使用fmt发起新的错误，默认severity为error
    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx,std::string_view fmt,Args&&... args) noexcept;

    /// 设置快触发函数
    void ALIB5_API set_fast_error_callback(RuntimeErrorTriggerFn fn) noexcept;
    /// 设置慢触发函数
    void ALIB5_API add_error_callback(RTErrorTriggerFnpp heavy_fn) noexcept;

    /// 杂类函数
    namespace misc{
        /// 返回格式化的当前时间
        std::string_view ALIB5_API get_time() noexcept;
        /// 格式化时间
        std::string_view ALIB5_API format_duration(int seconds) noexcept;
    }

    /// 字符串处理函数
    namespace str{
        /// 更加高级的去除转义语序
        std::string_view ALIB5_API unescape(std::string_view in) noexcept;
        /// 反向处理，对字符串进行转义化
        std::string_view ALIB5_API escape(std::string_view in) noexcept;
        /// 去除空白字符，返回的是input的sub string_view
        std::string_view ALIB5_API trim(std::string_view input);
        // 分割字符串，可以识别整个字符串,vector中为对source的切片
        std::vector<std::string_view> ALIB5_API split(std::string_view source,std::string_view seps) noexcept;
        /// 分割字符串,vector中为对source的切片
        inline std::vector<std::string_view> ALIB5_API split(std::string_view source,const char sep) noexcept{
            return str::split(source, std::string_view(&sep,1));
        }
        /// 大小写转换
        inline auto to_upper(std::string_view input){
            return input | std::views::transform([](const char & ch){
                return std::toupper(ch);
            });
        }
        inline auto to_lower(std::string_view input){
            return input | std::views::transform([](const char & ch){
                return std::tolower(ch);
            });
        }

        /// 简易的字符串常量池
        template<IsStringLike T>
        struct StringPool{
            std::pmr::unordered_set<
                T,
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > pool;

            StringPool(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
            :pool(__a){}

            inline std::string_view get(std::string_view input) requires
            requires(std::string_view str){
                T(str.begin(),str.end());
            }{
                auto it = pool.find(input);
                if(it != pool.end()){
                    return *it;
                }
                return *pool.emplace(T(input.begin(),input.end())).first;
            }
        }; 
    };

    /// 文件io函数
    namespace io{
        /// 读取文件
        template<CanExtendString T> size_t read_all(std::string_view path,T & output);
        /// 写入文件
        template<CanExtendString T> size_t write_all(std::string_view path,T & input);
        
        /// 保存了文件类型以及文件的名字
        /// 其中，对于directory（包括link），保证末尾是"/"或者"\\"(Windows)
        struct ALIB5_API FileEntry{
            /// 文件类型，摘自<filesystem>
            enum Type{
                none = 0     , not_found = -1, 
                regular = 1  , directory = 2, 
                symlink = 3  , block = 4,
                character = 5, fifo = 6,
                socket = 7   , unknown = 8
            };

            /// 移动函数
            inline void operator=(FileEntry && entry){
                path = std::move(entry.path);
                type = entry.type;
                subs = std::move(entry.subs);
                scanned = entry.scanned;
            }
            /// 复制函数
            inline void operator=(const FileEntry & entry){
                path = entry.path;
                type = entry.type;
                subs = entry.subs;
                scanned = entry.scanned;
            }
            inline FileEntry(FileEntry&& entry) noexcept{
                operator=(std::forward<FileEntry>(entry));
            }
            inline FileEntry(const FileEntry& entry) noexcept {
                *this = entry;
            }
            inline FileEntry() noexcept{}

            /// 是否有效
            inline bool invalid() const{
                return (type == not_found) || (!path.compare(""));
            }

            /// 重新扫描，适用于手动情况
            inline void rescan() const{
                scanned = false;
                scan_subs();
            }

            inline size_t file_size() const{
                std::error_code ec;
                size_t size = std::filesystem::file_size(path,ec);
                if(ec){
                    return std::variant_npos;
                }
                return size;
            }

            inline size_t elements_size() const{
                scan_subs();
                return subs.size();
            }

            inline auto begin() const{ scan_subs(); return subs.cbegin(); }
            inline auto end()const { scan_subs(); return subs.cend(); }
            inline auto begin(){ scan_subs(); return subs.begin(); }
            inline auto end(){ scan_subs(); return subs.end(); }

            /// 获取子目录
            const FileEntry& operator[](std::string_view str) const;
            /// 获取子目录，但是不需要进行立即扫描
            const FileEntry& operator()(std::string_view str) const;

            /// 读取数据
            template<CanExtendString T> size_t read(T & output,std::size_t read_count = 0) const;
            /// 读取数据，比较简单的版本，固定返回pmr
            inline std::pmr::string read(std::size_t read_count = 0) const{
                std::pmr::string str (ALIB5_DEFAULT_MEMORY_RESOURCE);
                read(str,read_count);
                return str;
            }
            /// 写入数据
            template<CanExtendString T> size_t write_all(T & output) const;

            /// 文件路径
            std::pmr::string path {ALIB5_DEFAULT_MEMORY_RESOURCE};
            /// 类型
            Type type {not_found};
        private:
            /// 缓存的子对象
            mutable std::pmr::unordered_map<std::pmr::string,FileEntry> subs {ALIB5_DEFAULT_MEMORY_RESOURCE};
            /// 缓存是否已经构建
            mutable bool scanned { false };
            /// 用于处理扁平化获取。一般大小不会很大
            mutable std::pmr::unordered_map<std::pmr::string,FileEntry> flat_subs {ALIB5_DEFAULT_MEMORY_RESOURCE};

            /// 生成不有效的数据
            static const FileEntry& gen_invalid();

            /// 搜索子目录
            void scan_subs() const;
        };

        FileEntry ALIB5_API load_entry(std::string_view) noexcept;

        /// 遍历目录的结果
        struct ALIB5_API TraverseData{
            // 对于root_absolute，必定以"/"或者"\" (为ALIB_PATH_SEPS)结尾，比如'C:\','/'
            /// 遍历的根目录的绝对路径
            std::pmr::string root_absolute;
            // 获取relative的方式为对path进行substr从而裁剪出相对路径
            /// 遍历的结果的绝对路径
            std::pmr::vector<FileEntry> targets_absolute;

            /// 获取entry中对应的相对路径，相对于root
            /// 这里你需要自己保证entry的来源是traverse data本身
            inline std::string_view try_get_relative(FileEntry & entry){
                return std::string_view(entry.path).substr(root_absolute.size());
            }
            /// 覆盖std使用场景
            using cont = std::pmr::vector<FileEntry>;
            using iterator = cont::iterator;
            inline auto begin(){return targets_absolute.begin();}
            inline auto end(){return targets_absolute.end();};
            inline auto begin() const{return targets_absolute.cbegin();}
            inline auto end() const{return targets_absolute.cend();};
        };

        /// 遍历文件的配置
        struct ALIB5_API TraverseConfig{
            /// 遍历深度，小于0表示遍历所有
            int depth { -1 }; // 遍历所有
            /// 基于遍历根目录，比如 traverse /home   --> "aaaa0ggmc" "aaaa0ggmc/..."
            std::vector<std::function<bool(std::string_view)>> ignore;
            /// 保留哪些类型
            std::vector<FileEntry::Type> keep { FileEntry::regular };

        private:
            /// 内部构建的东西
            friend TraverseData ALIB5_API traverse_files(std::string_view,TraverseConfig) noexcept;
            /// 构建allow数组
            void build() noexcept;
            /// 对于keep进行简易处理增加性能
            bool allow[10] {false};
        };


        /// 遍历文件
        /// @param file_path 支持相对路径和绝对路径
        TraverseData ALIB5_API traverse_files(std::string_view file_path,TraverseConfig config = TraverseConfig()) noexcept;
    };

    namespace sys{
    #ifdef _WIN32
        /// 启用系统的虚拟控制台，这样才能进行彩色转义输出
        ALIB5_API void enable_virtual_terminal() noexcept;
    #else
        // 一般都支持，所以留空
        /// 启用系统虚拟控制台（基本上都是支持的）
        inline void enable_virtual_terminal() noexcept{}
    #endif
        
        /// 获取系统的CPU的品牌名
        std::string_view ALIB5_API get_cpu_brand() noexcept;

        /// 内存占用结构体,单位byte
        struct ALIB5_API ProgramMemUsage{
            mem_bytes memory;       ///< 内存占用
            mem_bytes virt_memory;  ///< 虚拟内存占用
        };

        /// 全局内存占用结构体,单位byte
        struct ALIB5_API GlobalMemUsage{
            //In linux,percent = phyUsed / phyTotal
            unsigned int percent;     ///< 占用百分比,详情有注意事项 @attention Linux上等于 physicalUsed/physicalTotal,Windows上使用API提供的dwMemoryLoad
            mem_bytes physical_total; ///< 物理内存总值
            mem_bytes virtual_total;  ///< 虚拟内存总值，Linux上暂时固定为0
            mem_bytes physical_used;  ///< 物理内存占用值
            mem_bytes virtual_used;   ///< 虚拟内存占用值，Linux上暂时固定为0
            mem_bytes page_total;     ///< 页面文件总值，Linux上为SwapSize
            mem_bytes page_used;      ///< 页面文件占用值,Linux上为SwapUsed
        };


        /// 获取程序内存占用
        ProgramMemUsage ALIB5_API get_prog_mem_usage() noexcept;

        /// 获取系统内存占用
        GlobalMemUsage ALIB5_API get_global_mem_usage() noexcept;
    }
};

// 这里是统一的inline实现
namespace alib5{
    /// 写入文件
    template<CanExtendString T> size_t write_all(std::string_view path,T & output){
        // 这里主要是惧怕path是从一段字符串截取下来的，不构建string会导致输出错误
        FILE * f = std::fopen(std::string(path).c_str(),"wb");
        if(!f){
            invoke_error(err_io_error,"Failed to open file {}!",path);
            return std::variant_npos;
        }
        auto sz = std::fwrite(output.data(),sizeof(T::value_type),output.size(),f);
        std::fclose(f);
        return sz;
    }

    /// 写入文件
    template<CanExtendString T> size_t io::FileEntry::write_all(T & input) const{
        return write_all(path,input);
    }

    /// 内置读取文件
    template<CanExtendString T> inline size_t io::FileEntry::read(T & output,std::size_t read_count) const{
        if(!read_count)read_count = file_size();
        if(read_count == std::variant_npos){
            invoke_error(err_io_error,"File {} is invalid!",path);
            return std::variant_npos;
        }

        FILE * f = std::fopen(path.c_str(),"rb");
        if(f == NULL){
            invoke_error(err_io_error,"Failed to open file {}!",path);
            return std::variant_npos;
        }

        size_t old_size = output.size();
        output.resize(old_size + read_count);
        std::fread(output.data() + old_size,sizeof(char),read_count,f);
        std::fclose(f);
        return read_count;
    }

    /// 读取文件
    template<CanExtendString T> inline size_t read_all(std::string_view path,T & output){
        io::FileEntry entry = io::load_entry(path);
        auto size = entry.file_size();
        if(size == std::variant_npos){
            invoke_error(err_io_error,"File {} is invalid!",path);
            return size;
        }
        return entry.read(output,size);
    }

    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx,std::string_view fmt,Severity sv,Args&&... args) noexcept{
        try{
            std::pmr::string data(ALIB5_DEFAULT_MEMORY_RESOURCE);
            data.reserve(conf_error_string_reserve_size);
            std::vformat_to(std::back_inserter(data),fmt,std::make_format_args(args...));
            invoke_error(ctx, data,sv);
        }catch(...){
            invoke_error(ctx,fmt,sv);
        }
    }

    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx,std::string_view fmt,Args&&... args) noexcept{
        invoke_error(ctx,fmt,Severity::Error,std::forward<Args>(args)...);
    }

    namespace ext{
        template<IsStringLike T,bool copy> [[nodiscard]] auto to_string(T && v) noexcept{
            if constexpr(std::is_pointer_v<decltype(v)>){
                [[unlikely]] if(!v)return "";
            }
            fmt_buf.clear();
            fmt_buf = v;
            if constexpr(copy){
                return String(fmt_buf.c_str(),ALIB5_DEFAULT_MEMORY_RESOURCE);
            }else return std::string_view(fmt_buf);
        }

        template<class T,bool copy> [[nodiscard]] auto to_string(T && v) noexcept{
            fmt_buf.clear();
            try{
                std::format_to(std::back_inserter(fmt_buf),"{}",std::forward<T>(v));
            }catch(...){
                fmt_buf = "[FOMRAT_ERROR]";
                MAY_INVOKE(3){
                    invoke_error(err_format_error,"Failed to format the target!");
                }
            }
            if constexpr(copy){
                return String(fmt_buf.c_str(),ALIB5_DEFAULT_MEMORY_RESOURCE);
            }else return std::string_view(fmt_buf);
        }
    }

    inline std::string_view get_severity_str(Severity sv){
        switch(sv){
        case Severity::Trace:
            return "TRACE";
        case Severity::Debug:
            return "DEBUG";
        case Severity::Info:
            return "INFO ";
        case Severity::Warn:
            return "WARN ";
        case Severity::Error:
            return "ERROR";
        case Severity::Fatal:
            return "FATAL";
        default:
            return "?????";
        }
    }

    template<class T> inline auto ext::to_T(std::string_view v,std::from_chars_result * iresult) noexcept{
        auto report = [](std::error_code ec){
            if(ec){
                invoke_error(err_format_error,ec.message());
            }
        };
    
        if constexpr(std::is_same_v<T,bool>){
            // 适配一些形式,之所以不用转大小写是因为你不觉得 tRUe很诡异吗
            if(v == "true" || v == "1" || v == "TRUE" || v == "True")return true;
            if(v == "false" || v == "0" || v == "FALSE" || v == "False")return false;
            return false;
        }else if constexpr(std::is_arithmetic_v<T>){
            T val = T();
            auto result = std::from_chars(v.data(),v.data() + v.size(),val);
            if(iresult) *iresult = result;
            else report(std::make_error_code(result.ec));
            return val;
        }else if constexpr(IsStringLike<T>){
            return (std::string_view)v;
        }else{
            static_assert(IsStringLike<T>,"Failed to convert the value!");
        }
    }
}

#endif