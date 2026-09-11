/**
 * @file log.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 日志系统核心模块聚合导出
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.log;

export import :config;
export import :msg;
export import :mod;
export import :manip;
export import :targets;
export import :filters;
export import :fastfmt;
export import :stream;
export import :kernel;

export namespace alib6 {
    // 根命名空间直接暴露日志核心类型与配置 (遵循 alib5 规范)
    using LogLevel = alib6::log::LogLevel;
    using Severity = alib6::log::Severity;
    using alib6::log::get_log_level_name;
    using LogCustomTag = alib6::log::LogCustomTag;
    using LogMsgConfig = alib6::log::LogMsgConfig;
    using LoggerConfig = alib6::log::LoggerConfig;
    using LogFactoryConfig = alib6::log::LogFactoryConfig;

    using LogMsg = alib6::log::LogMsg;
    using LogEnd = alib6::log::LogEnd;

    using LogTarget = alib6::log::LogTarget;
    using LogFilter = alib6::log::LogFilter;
    using alib6::log::IsLogMod;
    using alib6::log::IsLogTarget;
    using alib6::log::IsLogFilter;

    using alib6::log::endlog;
    using alib6::log::fls;
    using alib6::log::tfmt;
    using alib6::log::log_tfmt;
    using alib6::log::log_source;
    using alib6::log::log_bin;
    using alib6::log::log_rate;
    using alib6::log::log_stacktrace;
    using alib6::log::log_tag;
    using alib6::log::log_omit;

    using alib6::log::StreamedContext;

    using alib6::log::Logger;
    using alib6::log::LogFactory;

    namespace lot = alib6::lot;
    namespace lof = alib6::lof;
}
