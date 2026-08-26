/**
 * @file concepts.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 常见概念集中定义
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:concepts;
import std;

export namespace alib6 {

    /// 封装 Convertible To 类型可转换概念
    template<class T, class V> 
    concept Cast = std::convertible_to<T, V>;

    /// 判定是否支持动态扩容并可写入字节的连续字符串容器
    template<class T> 
    concept CanExtendString = requires(T& t) {
        { t.data() } -> std::convertible_to<char*>;
        t.resize(static_cast<std::size_t>(1));
        { t.size() } -> std::convertible_to<std::size_t>;
        typename T::value_type;
    } && std::ranges::contiguous_range<T>;

}
