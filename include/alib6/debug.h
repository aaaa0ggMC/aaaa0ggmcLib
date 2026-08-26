/**
 * @file debug.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 断言与 Panic 宏工具箱（提供带条件表达式字符串化的宏辅助）
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once
#include <alib6/config.h>

#ifndef ALIB6_DISABLE_PANIC_MACROS

/// @brief 条件满足时 panic（附带 Condition 字符串化）
#define panic_if(COND, ARG) \
    do { \
        if (COND) [[unlikely]] { \
            ::alib6::panicf("{}\nCondition: {}", ARG, #COND); \
        } \
    } while (0)

/// @brief 条件满足时带格式化参数 panic
#define panicf_if(COND, STR, ...) \
    do { \
        if (COND) [[unlikely]] { \
            ::alib6::panicf(STR "\nCondition: " #COND, ##__VA_ARGS__); \
        } \
    } while (0)

/// @brief 条件断言（条件不满足时 panic）
#define panic_assert(COND) \
    do { \
        if (!(COND)) [[unlikely]] { \
            ::alib6::panicf("Assertion failed\nCondition: {}", #COND); \
        } \
    } while (0)

#ifdef NDEBUG
#define panic_debug(COND, ...)       do {} while (0)
#define panic_debug_if(COND, ...)    do {} while (0)
#define panicf_debug(COND, ...)      do {} while (0)
#define panic_debug_assert(COND)     do {} while (0)
#else
#define panic_debug(COND, ARG)       panic_if(COND, ARG)
#define panic_debug_if(COND, ARG)    panic_if(COND, ARG)
#define panicf_debug(COND, STR, ...) panicf_if(COND, STR, ##__VA_ARGS__)
#define panic_debug_assert(COND)     panic_assert(COND)
#endif

#endif
