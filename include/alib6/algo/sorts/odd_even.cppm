/**
 * @file odd_even.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 奇偶排序 (alib6.algo:sort.odd_even)
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

export module alib6.algo:sort.odd_even;

import :base;

export namespace alib6::algo {

    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void odd_even_sort(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool sorted = false;
        while (!sorted) {
            sorted = true;
            for (auto i = begin + 1; i < end - 1; i += 2) {
                if (compare(*(i + 1), *i, reverse)) {
                    std::swap(*i, *(i + 1));
                    inject(i.get_index(), (i + 1).get_index());
                    sorted = false;
                }
            }
            for (auto i = begin; i < end - 1; i += 2) {
                if (compare(*(i + 1), *i, reverse)) {
                    std::swap(*i, *(i + 1));
                    inject(i.get_index(), (i + 1).get_index());
                    sorted = false;
                }
            }
        }
    }

    template<
        IsRandomAccessIterator IterType,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void odd_even_sort(IterType i_begin, IterType i_end, bool reverse = false, InjectFn&& i_inject = nullptr) {
        odd_even_sort(i_begin, i_end, std::less<typename std::iterator_traits<IterType>::value_type>{}, reverse, std::forward<InjectFn>(i_inject));
    }

    template<class Range, class Comp = std::less<>, class Inject = std::nullptr_t>
    void odd_even_sort(Range& r, Comp comp = {}, bool reverse = false, Inject inject = nullptr) {
        odd_even_sort(std::begin(r), std::end(r), std::move(comp), reverse, std::move(inject));
    }

} // namespace alib6::algo
