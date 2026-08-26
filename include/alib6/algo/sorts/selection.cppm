/**
 * @file selection.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 选择排序 (alib6.algo:sort.selection)
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

export module alib6.algo:sort.selection;

import :base;

export namespace alib6::algo {

    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void selection_sort(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        for (auto i = begin; i < end - 1; ++i) {
            auto target = i;
            for (auto j = i + 1; j < end; ++j) {
                if (compare(*j, *target, reverse)) {
                    target = j;
                }
            }
            if (target != i) {
                std::swap(*i, *target);
                inject(i.get_index(), target.get_index());
            }
        }
    }

    template<
        IsRandomAccessIterator IterType,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void selection_sort(IterType i_begin, IterType i_end, bool reverse = false, InjectFn&& i_inject = nullptr) {
        selection_sort(i_begin, i_end, std::less<typename std::iterator_traits<IterType>::value_type>{}, reverse, std::forward<InjectFn>(i_inject));
    }

    template<class Range, class Comp = std::less<>, class Inject = std::nullptr_t>
    void selection_sort(Range& r, Comp comp = {}, bool reverse = false, Inject inject = nullptr) {
        selection_sort(std::begin(r), std::end(r), std::move(comp), reverse, std::move(inject));
    }

} // namespace alib6::algo
