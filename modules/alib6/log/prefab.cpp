/**
 * @file prefab.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6.log.prefab 预制日志系统实现
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

module alib6.log.prefab;
import std;
import alib6.core;
import alib6.log;

namespace alib6::log::prefab {

    Logger& get_default_logger() {
        static Logger default_instance(
            [] {
                LoggerConfig cfg;
                cfg.consumer_count = 0; // 默认同步模式保证调试测试即时输出
                return cfg;
            }()
        );
        static bool init_target = [] {
            default_instance.append_mod<Console>("console");
            return true;
        }();
        return default_instance;
    }

    LogFactory& get_default_aout_factory() {
        static LogFactory default_fac(get_default_logger(), [] {
            LogFactoryConfig cfg;
            cfg.msg.disable_extra_information = true;
            return cfg;
        }());
        return default_fac;
    }

    StreamedContext<LogFactory>&& _aout(bool auto_create) {
        thread_local static StreamedContext<LogFactory> context = get_default_aout_factory()();
        if (auto_create && context.context_used) {
            context.~StreamedContext();
            new (&context) StreamedContext(get_default_aout_factory()());
        }
        return std::move(context);
    }

} // namespace alib6::log::prefab
