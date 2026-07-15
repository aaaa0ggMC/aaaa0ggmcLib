/**
 * @file odd_even.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 奇偶排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_ODD_EVEN
#define ALIB5_AALGO_SORT_ODD_EVEN
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Odd-even sort (bubble sort variation).
     */
    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void odd_even(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool swapped = true;
    
        while (swapped) {
            swapped = false;
            for (auto i = begin; i < end - 1; i += 2) {
                if (compare(*(i + 1), *i, reverse)) {
                    std::swap(*(i + 1), *i);
                    inject((i + 1).get_index(), i.get_index());
                    swapped = true;
                }
            }
            for (auto i = begin + 1; i < end - 1; i += 2) {
                if (compare(*(i + 1), *i, reverse)) {
                    std::swap(*(i + 1), *i);
                    inject((i + 1).get_index(), i.get_index());
                    swapped = true;
                }
            }
        }
    }
}
#endif