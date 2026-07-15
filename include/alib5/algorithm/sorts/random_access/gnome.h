/**
 * @file gnome.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 地精排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_GNOME
#define ALIB5_AALGO_SORT_GNOME
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Gnome sort.
     * 
     * @par Algorithm Rules / 算法规律记录
     * If a > b, step forward. If a < b, swap and step backward.
     * / 树精排序 a > b 往前走, a < b 交换,往后走
     */
    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void gnome(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        
        auto current = begin;
        while (current < end - 1) {
            if (compare(*(current + 1), *current, reverse)) {
                std::swap(*current, *(current + 1));
                inject(current.get_index(), (current + 1).get_index());
                if (current != begin) --current;
                else ++current;
            } else {
                ++current;
            }
        }
    }
}

#endif