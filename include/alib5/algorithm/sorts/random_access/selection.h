/**
 * @file selection.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 选择排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_SELECTION
#define ALIB5_AALGO_SORT_SELECTION
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Simple selection sort. Time complexity O(n^2). Tier: E
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * a b c d e f g
     * Start from 'a' and move forward, finding the minimum subsequent element and putting it at the front.
     * / 从a开始往后走,每次把后面最小的放前面,然后就没了
     * @endcode
     */
    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void selection(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;

        for (auto current = begin; current != end; ++current) {
            auto front = current;
            for (auto inner = current + 1; inner != end; ++inner) {
                if (compare(*inner, *front, reverse)) {
                    front = inner;
                }
            }
            if (front != current) {
                std::swap(*front, *current);
                inject(front.get_index(), current.get_index());
            }
        }
    }
}

#endif