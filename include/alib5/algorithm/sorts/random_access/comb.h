/**
 * @file comb.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 梳子排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_COMB
#define ALIB5_AALGO_SORT_COMB
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Comb sort. Bubble sort improvement.
     * 
     * @par Algorithm Rules / 算法规律记录
     * A Bubble sort improvement utilizing a fixed shrink gap factor of 1.247.
     * / 依旧冒泡改良,梳排序. 选择了一个固定步长比例 1.247
     */
    template<
        float GapFactor = 1.247f,
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void comb(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        static_assert(GapFactor > 1, "Gap factor is required to be bigger than 1!");
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        size_t len = std::distance(begin, end);
        size_t gap = len;

        bool swapped = true;
        while (gap > 1 || swapped) {
            swapped = false;
            gap /= GapFactor;
            if (gap < 1) gap = 1;

            for (auto i = begin; i < end - gap; ++i) {
                if (compare(*(i + gap), *i, reverse)) {
                    std::swap(*i, *(i + gap));
                    inject(i.get_index(), (i + gap).get_index());
                    swapped = true;
                }
            }
        }
    }
}

#endif