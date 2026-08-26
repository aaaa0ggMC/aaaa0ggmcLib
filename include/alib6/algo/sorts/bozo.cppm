/**
 * @file bozo.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 猴子排序变体 (alib6.algo:sort.bozo)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <iterator>
#include <random>
#include <utility>

export module alib6.algo:sort.bozo;

import :base;

export namespace alib6::algo {

    template<
        std::size_t SwapsBetweenChecks = 31,
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void bozo_sort(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool mess = true;

        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<std::size_t> dist(0, std::distance(begin, end) - 1);

        while (true) {
            mess = false;
            for (auto i = begin; i < end - 1; ++i) {
                if (compare(*(i + 1), *i, reverse)) {
                    mess = true;
                    break;
                }
            }
            if (!mess) break;

            for (std::size_t i = 0; i < SwapsBetweenChecks; ++i) {
                std::size_t a = dist(generator);
                std::size_t b = dist(generator);
                while (b == a) [[unlikely]] b = dist(generator);

                std::swap(*(begin + b), *(begin + a));
                inject(b, a);
            }
        }
    }

    template<
        std::size_t SwapsBetweenChecks = 31,
        IsRandomAccessIterator IterType,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void bozo_sort(IterType i_begin, IterType i_end, bool reverse = false, InjectFn&& i_inject = nullptr) {
        bozo_sort<SwapsBetweenChecks>(i_begin, i_end, std::less<typename std::iterator_traits<IterType>::value_type>{}, reverse, std::forward<InjectFn>(i_inject));
    }

} // namespace alib6::algo
