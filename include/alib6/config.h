/**
 * @file config.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 基础配置与宏控制
 * @version 5.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#ifndef ALIB6_STR_FAILED_TO_FORMAT
#define ALIB6_STR_FAILED_TO_FORMAT "Failed to format target "
#endif

// 兼容拼写
#ifndef ALIB6_STR_FAILE_TO_FORMAT
#define ALIB6_STR_FAILE_TO_FORMAT ALIB6_STR_FAILED_TO_FORMAT
#endif

/// 异常机制支持 (全局总开关)
#if defined(ALIB6_USE_EXCEPTIONS) || defined(__cpp_exceptions)
#define ALIB6_FLAG_USE_EXCEPTIONS
#endif

/// ErrorWrapper 默认行为策略
/// - 默认策略：只要开启了异常支持，全部自动统一使用异常；无异常环境自动降级为 Panic。
/// - 特殊覆盖：可通过 ALIB6_ERROR_USE_PANIC 或 ALIB6_ERROR_IGNORE 强制改变 Error 的行为。
#if defined(ALIB6_ERROR_USE_PANIC)
#define ALIB6_FLAG_ERROR_PANIC
#elif defined(ALIB6_ERROR_IGNORE)
#define ALIB6_FLAG_ERROR_IGNORE
#elif defined(ALIB6_FLAG_USE_EXCEPTIONS)
#define ALIB6_FLAG_ERROR_EXCEPTION
#else
#define ALIB6_FLAG_ERROR_PANIC
#endif

/// 禁用严格类型位宽检查 (默认是开启的，定义此宏表示禁用)
#ifdef ALIB6_NO_STRICT_TYPE_CHECK
#define ALIB6_FLAG_NO_STRICT_TYPE_CHECK
#endif

/// 禁用 Stacktrace 调用栈回溯
#ifdef ALIB6_NO_STACKTRACE
#define ALIB6_FLAG_NO_STACKTRACE
#endif

/// 禁用 Panic 调试输出
#ifdef ALIB6_NO_DEBUG_OUTPUT
#define ALIB6_FLAG_NO_DEBUG_OUTPUT
#endif

/// 堆栈信息过滤 C++ 内部实现帧
#ifndef ALIB6_TRACE_SKIP_PROMPT
#define ALIB6_TRACE_SKIP_PROMPT "c++"
#endif

/// GLM 数学库扩展支持 (宏控制)
#ifndef ALIB6_DISABLE_GLM_EXTENSIONS
#if defined(GLM_ENABLE_EXPERIMENTAL) || defined(GLM_VERSION) || __has_include(<glm/glm.hpp>)
#define ALIB6_FLAG_ENABLE_GLM
#define ALIB6_HAS_GLM 1
#endif
#endif

/// RapidJSON 扩展支持
#ifndef ALIB6_DISABLE_RAPIDJSON_EXTENSIONS
#if defined(RAPIDJSON_HAS_STDSTRING) || defined(RAPIDJSON_VERSION_STRING) || __has_include(<rapidjson/document.h>)
#define ALIB6_FLAG_ENABLE_RAPIDJSON
#define ALIB6_HAS_RAPIDJSON 1
#endif
#endif

/// TOML++ 扩展支持
#ifndef ALIB6_DISABLE_TOMLPLUSPLUS_EXTENSIONS
#if defined(TOML_ENABLE_UNRELEASED_FEATURES) || defined(TOML_VERSION_MAJOR) || __has_include(<toml++/toml.hpp>)
#define ALIB6_FLAG_ENABLE_TOMLPLUSPLUS
#define ALIB6_HAS_TOMLPLUSPLUS 1
#endif
#endif
