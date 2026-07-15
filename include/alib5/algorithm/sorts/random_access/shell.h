/**
 * @file shell.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 希尔排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_SHELL
#define ALIB5_AALGO_SORT_SHELL
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /// @brief 不同的希尔排序类型
    enum class ShellGapType{ 
        Div2, 
        Knuth, 
        Hibbard 
    };

    /**
     * @brief [R] Simple shell sort. Time complexity O(n^1.3) - O(n^2). Tier: D
     * 
     * @par Notes / 备注:
     * Basically insertion sort with a gap. / 就是带上gap的插入排序
     */
    template<
        ShellGapType GapType = ShellGapType::Div2,
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void shell(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;

        size_t len = std::distance(begin, end);
        size_t gap = 1;

        if constexpr(GapType == ShellGapType::Div2) {
            gap = len / 2;
        } else if constexpr(GapType == ShellGapType::Knuth) {
            while (gap < len / 3) gap = 3 * gap + 1;
        } else if constexpr(GapType == ShellGapType::Hibbard) {
            while (gap <= len) gap = (gap << 1) | 1;
            gap >>= 2;
            if (gap == 0) gap = 1;
        }

        auto gap_fn = [&]() {
            if constexpr(GapType == ShellGapType::Div2) {
                gap >>= 1;
            } else if constexpr(GapType == ShellGapType::Knuth) {
                gap = gap / 3;
            } else if constexpr(GapType == ShellGapType::Hibbard) {
                gap >>= 1;
            }
        };

        while (gap > 0) {
            for (size_t begin_pos = 0; begin_pos < gap; ++begin_pos) {
                for (size_t c_begin = begin_pos + gap; c_begin < len; c_begin += gap) {
                    auto s_begin = begin + c_begin;
                    auto j = s_begin;
                    value_t val = std::move(*s_begin);

                    while (j > (begin + begin_pos) && compare(val, *(j - gap), reverse)) {
                        *j = std::move(*(j - gap));
                        inject((j - gap).get_index(), j.get_index());
                        j -= gap;
                    }
                    *j = std::move(val);
                }
            }
            gap_fn();
        }
    }
}

#endif