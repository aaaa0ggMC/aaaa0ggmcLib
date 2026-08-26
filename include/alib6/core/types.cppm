/**
 * @file types.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 严格定长的基本数值与系统类型别名
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:types;
import std;

export namespace alib6 {

    // 定长有符号整数 (Fixed-width Signed Integers)
    using i8    = std::int8_t;
    using i16   = std::int16_t;
    using i32   = std::int32_t;
    using i64   = std::int64_t;
    using isize = std::ptrdiff_t;
    using iptr  = std::intptr_t;

    // 定长无符号整数 (Fixed-width Unsigned Integers)
    using u8    = std::uint8_t;
    using u16   = std::uint16_t;
    using u32   = std::uint32_t;
    using u64   = std::uint64_t;
    using usize = std::size_t;
    using uptr  = std::uintptr_t;

    // 128 位整数支持 (128-bit Integers, GCC/Clang 扩展)
#if defined(__SIZEOF_INT128__)
    using i128  = __int128_t;
    using u128  = __uint128_t;
#endif

    // 定长浮点类型 (Fixed-width Floating Point Types, C++23/C++26 <stdfloat>)
    
#if defined(__STDCPP_FLOAT16_T__)
    using f16   = std::float16_t;
#endif

#if defined(__STDCPP_BFLOAT16_T__)
    using bf16  = std::bfloat16_t;
#endif

#if defined(__STDCPP_FLOAT32_T__)
    using f32   = std::float32_t;
#else
    using f32   = float;
#endif

#if defined(__STDCPP_FLOAT64_T__)
    using f64   = std::float64_t;
#else
    using f64   = double;
#endif

#if defined(__STDCPP_FLOAT128_T__)
    using f128  = std::float128_t;
#elif defined(__SIZEOF_FLOAT128__) || (defined(__GNUC__) && defined(__x86_64__))
    using f128  = __float128;
#endif

    // 字节与内存大小别名
    using byte      = std::byte;
    using mem_bytes = std::size_t;

    // 编译期位宽断言 (默认开启，定义 ALIB6_NO_STRICT_TYPE_CHECK 可禁用)
#ifndef ALIB6_FLAG_NO_STRICT_TYPE_CHECK
    static_assert(sizeof(i8) == 1,   "i8 must be 1 byte");
    static_assert(sizeof(i16) == 2,  "i16 must be 2 bytes");
    static_assert(sizeof(i32) == 4,  "i32 must be 4 bytes");
    static_assert(sizeof(i64) == 8,  "i64 must be 8 bytes");

    static_assert(sizeof(u8) == 1,   "u8 must be 1 byte");
    static_assert(sizeof(u16) == 2,  "u16 must be 2 bytes");
    static_assert(sizeof(u32) == 4,  "u32 must be 4 bytes");
    static_assert(sizeof(u64) == 8,  "u64 must be 8 bytes");

    static_assert(sizeof(f32) == 4,  "f32 must be 4 bytes");
    static_assert(sizeof(f64) == 8,  "f64 must be 8 bytes");
    static_assert(sizeof(byte) == 1, "byte must be 1 byte");

#if defined(__SIZEOF_INT128__)
    static_assert(sizeof(i128) == 16, "i128 must be 16 bytes");
    static_assert(sizeof(u128) == 16, "u128 must be 16 bytes");
#endif

#if defined(__STDCPP_FLOAT16_T__)
    static_assert(sizeof(f16) == 2,  "f16 must be 2 bytes");
#endif
#if defined(__STDCPP_BFLOAT16_T__)
    static_assert(sizeof(bf16) == 2, "bf16 must be 2 bytes");
#endif
#if defined(f128) || defined(__STDCPP_FLOAT128_T__) || defined(__SIZEOF_FLOAT128__)
    static_assert(sizeof(f128) == 16, "f128 must be 16 bytes");
#endif
#endif

}
