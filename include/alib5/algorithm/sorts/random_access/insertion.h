/**
 * @file insertion.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 插入排序实现
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_INSERTION
#define ALIB5_AALGO_SORT_INSERTION
#include <alib5/algorithm/sorts/base.h>

namespace alib5::algo::sort{
    /**
     * @brief [R] Basic insertion sort. Time complexity O(n^2). Tier: D
     * 
     * @tparam IterType Random access iterator type.
     * @tparam CompareFn Comparison functor type.
     * @tparam InjectFn Injection hook type.
     * @param i_begin Start of the sequence.
     * @param i_end End of the sequence.
     * @param i_compare Comparison function.
     * @param reverse Reverse sort (descending).
     * @param i_inject Injection hook for swaps.
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * a b c d e f g
     * (Starting from a b), since a is already sorted at the beginning,
     * treat b as the empty region and compare a and b. Since a is sorted, 
     * the insertion region for b continues to move forward until it encounters an element where the comparison with b is false.
     * / (从a b开始),因为a开始时a已经是排完序了的
     * 此时把b看成空区域,比较a b,因为a已经排序完毕,因此往后推移是b该插入的区域不断往前,直到遇到和b比较为false的
     * 
     * For example, sorting 3 2 1: / 比如 3 2 1 排序
     * 3 2: Since 3 > 2, 3 is placed where 2 was. Result is 2 3, then stops.
     * / 3 2 因为 3 > 2因此3放到2处,有结果为 2 3,此时结束
     * 2 3 1: Since 3 > 1, the insertion position for 1 moves forward, yielding 2 1 3.
     * / 2 3 1 因为 3 > 1 因此插入1的位置往前,有 2 1 3
     * 2 1 3: Since 2 > 1, the insertion position for 1 moves forward again, yielding 1 2 3.
     * / 2 1 3 因为 2 > 1 因此插入1位置往前,有1 2 3
     * @endcode
     */
    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void insertion(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        
        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;

        for (auto current = (begin + 1); current < end; ++current) {
            value_t val = std::move(*current);
            auto j = current;

            while (j > begin && compare(val, *(j - 1), reverse)) {
                *j = std::move(*(j - 1));
                inject(j.get_index(), (j - 1).get_index());
                --j;
            }
            *j = std::move(val);
        }
    }
}

#endif