/**
 * @file string_format.h
 * @brief Thread-local string formatter reusing the full alogger pipeline. / 使用完整日志管线进行格式化的字符串格式化器
 * @author aaaa0ggmc
 * @date 2026/07/28
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef ALIB5_COMPACT_STRING_FORMAT_H
#define ALIB5_COMPACT_STRING_FORMAT_H
#include <alib5/alogger.h>

namespace alib5::compact {

    namespace detail{
        struct StringTarget : LogTarget {
            std::pmr::string data;

            StringTarget(std::pmr::memory_resource * __a)
            :data(__a){}

            void write(LogMsg & msg) override {
                data = std::move(msg.body);
            }
        };
    }

    struct StringFormatter{
        struct Impl{
            Logger logger;
            LogFactory lg;
            std::shared_ptr<detail::StringTarget> target;

            static LoggerConfig make_logger_cfg(){
                LoggerConfig cfg;
                cfg.consumer_count = 0;
                return cfg;
            }

            static LogFactoryConfig make_lg_cfg(){
                LogFactoryConfig lgcfg;
                lgcfg.msg.disable_extra_information = true;
                return lgcfg;
            }

            Impl()
            :logger(make_logger_cfg())
            ,lg(logger,make_lg_cfg()){
                target = logger.append_mod<detail::StringTarget>(
                    "_fmt_internal",
                    &logger.msg_buf
                );
            }
        };

        inline static thread_local Impl impl;

        template<class Fn>
        std::pmr::string operator()(Fn && fn) {
            impl.target->data.clear();
            {
                auto ctx = impl.lg();
                std::forward<Fn>(fn)(ctx);
            }
            auto & data = impl.target->data;
            if (!data.empty() && data.back() == '\n')
                data.pop_back();
            return std::move(data);
        }
    };

    inline static thread_local StringFormatter string_fmt;

}

#endif