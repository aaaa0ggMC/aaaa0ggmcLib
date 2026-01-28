/**@file autil.h
* @brief 工具库，提供实用函数
* @author aaaa0ggmc
* @date 2026/01/28
* @version 5.0
* @copyright Copyright(c) 2026
*/
#ifndef ALIB5_AUTIL
#define ALIB5_AUTIL
#include <alib5/detail/abase.h>
#include <alib5/detail/aerr_id.h>
#include <concepts>
#include <cstdint>
#include <iterator>
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

/// 虽然其实这个没啥用，但是这个还是指定了默认情况下alib5使用的内存资源
#define ALIB5_DEFAULT_MEMORY_RESOURCE std::pmr::get_default_resource()
#define CAT(a,b) a##b
#define CAT_2(a,b) CAT(a,b)
#define defer alib5::defer_t CAT_2(defer,__COUNTER__) = [&] noexcept  
#define MAY_INVOKE(N) if constexpr(conf_lib_error_invoke_level >= (N))

namespace alib5{
    /// 默认的错误推送预留大小
    constexpr uint32_t conf_error_string_reserve_size = 64;
    /// Level 0 : 没有输出
    /// Level 1 : 致命错误的时候输出
    /// Level 2 : 轻伤输出
    /// Level 3 : 输出一切
    constexpr uint32_t conf_lib_error_invoke_level = 1;
    /// 判断输入内容是否可以归类于string
    template<class T> concept IsStringLike = std::convertible_to<T,std::string_view>;
    using String = std::pmr::string;

    namespace detail{
        // 用于触发implicit调用方便处理
        /// 错误信息
        struct ALIB5_API ErrorCtx{
            int64_t id;                 ///< 错误的Id
            std::source_location loc;   ///< 错误发生地点
            
            /// 你只需要传递int64_t id通过implicit constructor生成对象
            ErrorCtx(int64_t iid,std::source_location isl = std::source_location::current()):id(iid),loc(isl){}
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

    /// 字符串处理函数
    namespace str{

    };

    /// 文件io函数
    namespace io{
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
            
            /// 文件路径
            std::pmr::string path;
            /// 类型
            Type type;
        };

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
        ALIB5_API void enable_virtual_terminal();
    #else
        // 一般都支持，所以留空
        /// 启用系统虚拟控制台（基本上都是支持的）
        inline void enable_virtual_terminal(){}
    #endif
        
        /// 获取系统的CPU的品牌名
        std::string_view get_cpu_brand();
    }
};

// 这里是统一的inline实现
namespace alib5{
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
}

#endif