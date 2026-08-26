/**
 * @file insertion.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 插入排序 (alib6.algo:sort.insertion)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <iterator>
#include <utility>

export module alib6.algo:sort.insertion;

import :base;

export namespace alib6::algo {

    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void insertion_sort(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        for (auto i = begin + 1; i < end; ++i) {
            auto key = std::move(*i);
            auto j = i - 1;
            while (j >= begin && compare(key, *j, reverse)) {
                *(j + 1) = std::move(*j);
                inject(j.get_index(), (j + 1).get_index());
                --j;
            }
            *(j + 1) = std::move(key);
        }
    }

    template<
        IsRandomAccessIterator IterType,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void insertion_sort(IterType i_begin, IterType i_end, bool reverse = false, InjectFn&& i_inject = nullptr) {
        insertion_sort(i_begin, i_end, std::less<typename std::iterator_traits<IterType>::value_type>{}, reverse, std::forward<InjectFn>(i_inject));
    }

    template<class Range, class Comp = std::less<>, class Inject = std::nullptr_t>
    void insertion_sort(Range& r, Comp comp = {}, bool reverse = false, Inject inject = nullptr) {
        insertion_sort(std::begin(r), std::end(r), std::move(comp), reverse, std::move(inject));
    }

} // namespace alib6::algo
