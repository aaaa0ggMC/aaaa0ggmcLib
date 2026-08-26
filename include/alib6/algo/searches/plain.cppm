/**
 * @file plain.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 朴素模式匹配 (alib6.algo:search.plain)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <string_view>
#include <iterator>
#include <concepts>
#include <cstddef>
#include <ranges>

export module alib6.algo:search.plain;

import :base;

export namespace alib6::algo {

    /**
     * @brief 泛型迭代器朴素模式匹配
     */
    template<IsGenericIterator T1, IsGenericIterator T2>
        requires IsComparableIterators<T1, T2>
    T1 plain_search(T1 data_begin, T1 data_end, T2 pattern_begin, T2 pattern_end) {
        if (pattern_begin == pattern_end) {
            return data_begin;
        }

        if constexpr (std::sized_sentinel_for<T1, T1> && std::sized_sentinel_for<T2, T2>) {
            auto data_size = std::distance(data_begin, data_end);
            auto pattern_size = std::distance(pattern_begin, pattern_end);

            if (pattern_size > data_size) {
                return data_end;
            }
        }

        for (T1 current = data_begin; current != data_end; ++current) {
            T1 match_current = current;
            T2 pattern_current = pattern_begin;

            while (true) {
                if (pattern_current == pattern_end) {
                    return current;
                }
                if (match_current == data_end) {
                    return data_end;
                }
                if (*match_current != *pattern_current) {
                    break;
                }

                ++match_current;
                ++pattern_current;
            }
        }
        return data_end;
    }

    /**
     * @brief 基于 Range 容器的朴素查找
     * @return 匹配成功的下标索引，若未找到返回 std::string_view::npos
     */
    template<class TextRange, class PatternRange>
    [[nodiscard]] std::size_t plain_search(const TextRange& text, const PatternRange& pattern) {
        if constexpr (requires { std::string_view(text).find(std::string_view(pattern)); }) {
            std::string_view t_sv(text);
            std::string_view p_sv(pattern);
            return t_sv.find(p_sv);
        } else {
            auto d_begin = std::ranges::begin(text);
            auto d_end = std::ranges::end(text);
            auto p_begin = std::ranges::begin(pattern);
            auto p_end = std::ranges::end(pattern);

            auto match = plain_search(d_begin, d_end, p_begin, p_end);
            if (match == d_end) return std::string_view::npos;
            return std::distance(d_begin, match);
        }
    }

} // namespace alib6::algo
