/**
 * @file shell.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 希尔排序 (alib6.algo:sort.shell)
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

export module alib6.algo:sort.shell;

import :base;

export namespace alib6::algo {

    template<
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void shell_sort(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        std::ptrdiff_t n = std::distance(begin, end);

        for (std::ptrdiff_t gap = n / 2; gap > 0; gap /= 2) {
            for (auto i = begin + gap; i < end; ++i) {
                auto temp = std::move(*i);
                auto j = i;
                while (j >= begin + gap && compare(temp, *(j - gap), reverse)) {
                    *j = std::move(*(j - gap));
                    inject((j - gap).get_index(), j.get_index());
                    j -= gap;
                }
                *j = std::move(temp);
            }
        }
    }

    template<
        IsRandomAccessIterator IterType,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void shell_sort(IterType i_begin, IterType i_end, bool reverse = false, InjectFn&& i_inject = nullptr) {
        shell_sort(i_begin, i_end, std::less<typename std::iterator_traits<IterType>::value_type>{}, reverse, std::forward<InjectFn>(i_inject));
    }

    template<class Range, class Comp = std::less<>, class Inject = std::nullptr_t>
    void shell_sort(Range& r, Comp comp = {}, bool reverse = false, Inject inject = nullptr) {
        shell_sort(std::begin(r), std::end(r), std::move(comp), reverse, std::move(inject));
    }

} // namespace alib6::algo
