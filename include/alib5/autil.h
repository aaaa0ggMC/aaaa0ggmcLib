/**@file autil.h
* @brief 工具库，提供实用函数
* @author aaaa0ggmc
* @date 2026/01/21
* @version 5.0
* @copyright Copyright(c) 2026
*/
#ifndef ALIB5_AUTIL
#define ALIB5_AUTIL
#include <alib5/abase.h>
#include <alib5/aerr_id.h>
#include <concepts>
#include <cstdint>
#include <iterator>
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
    constexpr uint32_t conf_error_string_reserve_size = 64;
    /// Level 0 : 没有输出
    /// Level 1 : 致命错误的时候输出
    /// Level 2 : 轻伤输出
    /// Level 3 : 输出一切
    constexpr uint32_t conf_lib_error_invoke_level = 1;
    
    template<class T> concept IsStringLike = std::convertible_to<T,std::string_view>;
    using String = std::pmr::string;

    namespace detail{
        struct ALIB5_API ErrorCtx{
            int64_t id;
            std::source_location loc;
            
            ErrorCtx(int64_t iid,std::source_location isl = std::source_location::current()):id(iid),loc(isl){}
        };     
    }

    namespace ext{
        static inline thread_local String fmt_buf(ALIB5_DEFAULT_MEMORY_RESOURCE); 

        template<IsStringLike T,bool copy = true> [[nodiscard]] auto to_string(T && v) noexcept;
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

    struct ALIB5_API RuntimeError{
        int64_t id;
        std::string_view data;
        Severity severity;
        std::source_location location;
    };   

    typedef void(*RuntimeErrorTriggerFn)(const RuntimeError & );
    using RTErrorTriggerFnpp = std::function<void(const RuntimeError &)>;

    void ALIB5_API invoke_error(detail::ErrorCtx ctx,std::string_view data = "",Severity = Severity::Error) noexcept;
    template<class... Args>
    void invoke_error(detail::ErrorCtx ctx,std::string_view fmt,Severity,Args&&... args) noexcept;
    template<class... Args>
    inline void invoke_error(detail::ErrorCtx ctx,std::string_view fmt,Args&&... args) noexcept;

    void ALIB5_API set_fast_error_callback(RuntimeErrorTriggerFn fn) noexcept;
    void ALIB5_API add_error_callback(RTErrorTriggerFnpp heavy_fn) noexcept;

    namespace str{

    };

    namespace io{
        
    };

    namespace sys{
    #ifdef _WIN32
        ALIB5_API void enable_virtual_terminal();
    #else
        /// 一般都支持，所以留空
        inline void enable_virtual_terminal(){}
    #endif
        
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
            std::vformat_to(std::back_inserter(data),fmt,std::make_format_args<Args...>(std::forward<Args>(args)...));
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
    };
}

#endif