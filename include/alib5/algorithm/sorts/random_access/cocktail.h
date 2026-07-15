/**
 * @file cocktail.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 鸡尾酒排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_COCKTAIL
#define ALIB5_AALGO_SORT_COCKTAIL
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Cocktail shaker sort.
     * 
     * @par Algorithm Rules / 算法规律记录
     * Bidirectional bubble sort: bubbles left, then right.
     * Simple optimization using a swappable flag. If no swap occurs, sorting is complete.
     * / 鸡尾酒排序,左边冒泡一次右边冒泡一次. 简单优化:swappable,没有swappable便说明排序完毕了
     */
    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void cocktail(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool swapped = true;
    
        auto high_sorted = end;
        auto low_sorted = begin;
        while (low_sorted < high_sorted && swapped) {
            swapped = false;

            for (auto i_current = low_sorted; i_current < high_sorted - 1; ++i_current) {
                if (compare(*(i_current + 1), *i_current, reverse)) {
                    std::swap(*i_current, *(i_current + 1));
                    inject(i_current.get_index(), (i_current + 1).get_index());
                    swapped = true;
                }
            }
            --high_sorted;
            
            if (!swapped) [[unlikely]] break;

            for (auto i_current = high_sorted - 1; i_current > low_sorted; --i_current) {
                if (compare(*i_current, *(i_current - 1), reverse)) {
                    std::swap(*i_current, *(i_current - 1));
                    inject(i_current.get_index(), (i_current - 1).get_index());
                    swapped = true;
                }
            }
            ++low_sorted;
        }
    }
}

#endif