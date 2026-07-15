/**
 * @file bubble.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 冒泡排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_BUBBLE
#define ALIB5_AALGO_SORT_BUBBLE
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Basic bubble sort. Time complexity O(n^2). Tier: E
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * a b c d e f g
     * Starting from index 0, if a > next, swap. Do this (n - (index + 1)) times for (n - 1) rounds.
     * Optimization: use a `swapped` flag to exit early if no swaps are made.
     * / 从0号位开始如果 a > 下一个就交换持续,执行(n-(index+1))(index从0开始)次,执行(n-1)轮次
     * 其他的类似
     * 可引入优化 swapped,如果swapped为false就可以提前跑了
     * For example, sorting 3 2 1: / 比如 3 2 1 排序
     * 3 > 2 swap: 2 3 1
     * 3 > 1 swap: 2 1 3
     * 2 > 1 swap: 1 2 3
     * It has executed (n-2) times here, stop swapping for this iteration. 
     * Loop has run (n-1) rounds. Exit.
     * / 此时执行了(n-2)次了,因此不要进行交换了
     * / 已经执行(n-1)轮.离开
     * @endcode
     */
    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void bubble(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool swapped = true;
    
        for (auto current = begin; current < (end - 1) && swapped; ++current) {
            swapped = false;
            for (auto i_current = begin; i_current < (end - 1 - (current.get_index())); ++i_current) {
                if (compare(*(i_current + 1), *i_current, reverse)) {
                    std::swap(*i_current, *(i_current + 1));
                    inject(i_current.get_index(), (i_current + 1).get_index());
                    swapped = true;
                }
            }
        }
    }
}
#endif