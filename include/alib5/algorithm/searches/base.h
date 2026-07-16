/**
 * @file base.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些基础的内容
 * @version 5.0
 * @date 2026-07-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SEARCH_BASE
#define ALIB5_AALGO_SEARCH_BASE
#include <iterator>
#include <limits>
#include <concepts>

namespace alib5::algo::search{
    /// 找不到东西
    constexpr size_t search_npos = std::numeric_limits<size_t>::max();

    /**
     * @brief Concept for random access iterators and pointers.
     */
    template<class T> 
    concept IsRandomAccessIterator = std::random_access_iterator<T> || std::is_pointer_v<T>;

    /**
     * @brief Concept for forward iterators.
     */
    template<class T> 
    concept IsForwardIterator = std::forward_iterator<T>;
    
    /**
     * @brief Concept for bidirectional iterators.
     */
    template<class T> 
    concept IsBiDirectionalIterator = std::bidirectional_iterator<T>;

    /**
     * @brief Generic iterator concept supporting forward, bidirectional, and random access patterns.
     * 
     * @par Notes / 备注:
     * Supports random access and forward access. / 支持随机访问和forward访问
     */
    template<class T> 
    concept IsGenericIterator = IsBiDirectionalIterator<T> || IsForwardIterator<T> || IsRandomAccessIterator<T>;

    /// 数值可以进行匹配的迭代器
    template<class T1,class T2> 
    concept IsComparableIterators = requires(T1 & a,T2 & b){
        { (*a) != (*b) } -> std::convertible_to<bool>;
    };
}

#endif