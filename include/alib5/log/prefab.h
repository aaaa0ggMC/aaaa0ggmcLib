#ifndef ALIB5_ALOGGER_PREFAB
#define ALIB5_ALOGGER_PREFAB
#include <alib5/log/kernel.h>
#include <alib5/log/targets.h>

namespace alib5{
    /// 返回内置的logger
    Logger& ALIB5_API __get_internal_logger();
    
    inline static Logger& alogger = __get_internal_logger();
    inline static LogFactory __aout (__get_internal_logger(),[]{
        LogFactoryConfig cfg;
        cfg.msg.disable_extra_information = true;
        return cfg;
    }());

    inline StreamedContext<LogFactory>&& _aout(bool auto_create = true){
        thread_local static StreamedContext<LogFactory> context = __aout();
        if(auto_create && context.context_used){
            context.~StreamedContext();
            new (&context) StreamedContext(__aout());
        }
        return std::move(context);
    }

    struct _AOUT_PROXY {
        template<class T> decltype(auto) operator<<(T && t){
            using type = decltype(_aout() << std::forward<T>(t));
            if constexpr(std::is_same_v<type,StreamedContext<LogFactory>&&>) {
                return std::move(_aout() << std::forward<T>(t));
            }else {
                return _aout() << std::forward<T>(t);
            }
        }

        void flush(){
            auto && ctx = _aout();
            std::move(ctx) << fls;
        }

        ~_AOUT_PROXY(){
            auto && ctx = _aout(false);
            if(!ctx.context_used && ctx.context_valid)std::move(ctx) << endlog;
        }
    };

    inline static _AOUT_PROXY aout;
}


#endif