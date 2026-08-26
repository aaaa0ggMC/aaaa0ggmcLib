/**
 * @file kmp.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief KMP 模式匹配算法与预计算上下文 KmpContext (alib6.algo:search.kmp)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <string_view>
#include <vector>
#include <iterator>
#include <concepts>
#include <cstddef>
#include <ranges>

export module alib6.algo:search.kmp;

import alib6.core;
import :base;

namespace pmr = std::pmr;

export namespace alib6::algo {

    /**
     * @brief KMP 预计算模式上下文 (KmpContext)
     * 提前计算并缓存失配表 (next 数组)，支持跨多次匹配高效复用与 PMR 内存隔离。
     * 
     * @tparam T 模式串迭代器类型 (必须满足 IsRandomAccessIterator)
     */
    template<IsRandomAccessIterator T>
    struct KmpContext {
        pmr::vector<std::size_t> next;
        T pattern_begin{};
        T pattern_end{};

        explicit KmpContext(memory_resource* mem = get_default_resource())
            : next(mem) {}

        KmpContext(T begin, T end, memory_resource* mem = get_default_resource())
            : next(mem), pattern_begin(begin), pattern_end(end) {
            calculate();
        }

        template<std::ranges::random_access_range R>
        explicit KmpContext(const R& range, memory_resource* mem = get_default_resource())
            : next(mem), pattern_begin(std::ranges::begin(range)), pattern_end(std::ranges::end(range)) {
            calculate();
        }

        void calculate() {
            std::size_t pattern_size = std::distance(pattern_begin, pattern_end);
            next.assign(pattern_size, 0);
            if (pattern_size <= 1) return;

            std::size_t j = 0;
            for (std::size_t i = 1; i < pattern_size; ++i) {
                while (j > 0 && *(pattern_begin + i) != *(pattern_begin + j)) {
                    j = next[j - 1];
                }
                if (*(pattern_begin + i) == *(pattern_begin + j)) {
                    ++j;
                }
                next[i] = j;
            }
        }
    };

    template<std::ranges::random_access_range R>
    KmpContext(const R&) -> KmpContext<std::ranges::iterator_t<const R>>;

    template<std::ranges::random_access_range R>
    KmpContext(const R&, memory_resource*) -> KmpContext<std::ranges::iterator_t<const R>>;

    /**
     * @brief 基于预计算 KmpContext 执行 KMP 查找
     */
    template<IsRandomAccessIterator T1, IsRandomAccessIterator T2>
        requires IsComparableIterators<T1, T2>
    T1 kmp_search(T1 data_begin, T1 data_end, const KmpContext<T2>& context) {
        T2 pattern_begin = context.pattern_begin;
        std::size_t pattern_size = context.next.size();
        const auto& next = context.next;

        std::size_t data_size = std::distance(data_begin, data_end);
        if (pattern_size > data_size) {
            return data_end;
        }
        if (pattern_size == 0) {
            return data_begin;
        }

        std::size_t j = 0;
        for (std::size_t i = 0; i < data_size;) {
            if (*(data_begin + i) == *(pattern_begin + j)) {
                ++i;
                ++j;
                if (j == pattern_size) {
                    return data_begin + (i - j);
                }
            } else if (j > 0) {
                j = next[j - 1];
            } else {
                ++i;
            }
        }
        return data_end;
    }

    /**
     * @brief 直接传入模式串范围的 KMP 查找 (内部构造 KmpContext)
     */
    template<IsRandomAccessIterator T1, IsRandomAccessIterator T2>
        requires IsComparableIterators<T1, T2>
    T1 kmp_search(T1 data_begin, T1 data_end, T2 pattern_begin, T2 pattern_end) {
        return kmp_search(data_begin, data_end, KmpContext<T2>(pattern_begin, pattern_end));
    }

    /**
     * @brief 基于 Range 容器与 KmpContext 的高层匹配接口
     * @return 匹配成功的下标索引，若未找到返回 std::string_view::npos
     */
    template<std::ranges::random_access_range DataRange, IsRandomAccessIterator T2>
    [[nodiscard]] std::size_t kmp_search(const DataRange& data, const KmpContext<T2>& context) {
        auto begin = std::ranges::begin(data);
        auto end = std::ranges::end(data);
        auto match = kmp_search(begin, end, context);
        if (match == end) return std::string_view::npos;
        return std::distance(begin, match);
    }

    /**
     * @brief 基于 Range 容器的高层匹配接口
     * @return 匹配成功的下标索引，若未找到返回 std::string_view::npos
     */
    template<std::ranges::random_access_range DataRange, std::ranges::random_access_range PatternRange>
    [[nodiscard]] std::size_t kmp_search(const DataRange& data, const PatternRange& pattern) {
        auto p_begin = std::ranges::begin(pattern);
        auto p_end = std::ranges::end(pattern);
        KmpContext<decltype(p_begin)> ctx(p_begin, p_end);
        return kmp_search(data, ctx);
    }

} // namespace alib6::algo
