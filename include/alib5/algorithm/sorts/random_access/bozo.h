/**
 * @file bozo.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 猴子排序变体
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_BOZO
#define ALIB5_AALGO_SORT_BOZO
#include <alib5/algorithm/sorts/base.h>
#include <random>

namespace alib5::algo::sort{
    /**
     * @brief [R] Bozo sort (swap version). 
     * 
     * @par Notes / 备注:
     * Since `std::shuffle` cannot execute an injection hook, random swapping is used instead.
     * It is recommended to add a timeout check in the inject function and throw an exception to be caught externally.
     * Also, `SwapsBetweenChecks` should ideally be an odd number, otherwise when there are only two elements, an even number of swaps results in no change.
     * / 猴子排序,交换版本,主要是shuffle无法进行inject
     * 建议inject函数加入一个超时检测然后 throw exception,这样外部就可以捕捉异常了
     * 同时建议swapbetchecks为奇数,不然只有两个元素时换了一轮和没有一样
     */
    template<
        size_t SwapsBetweenChecks = 31,
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void bozo(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
         auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool mess = true;

        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<size_t> dist(0, std::distance(begin, end) - 1);

        while (true) {
            mess = false;
            for (auto i = begin; i < end - 1; ++i) {
                if (compare(*(i + 1), *i, reverse)) {
                    mess = true;
                    break;
                }
            }
            if (!mess) break;

            for (size_t i = 0; i < SwapsBetweenChecks; ++i) {
                size_t a = dist(generator);
                size_t b = dist(generator);
                while (b == a) [[unlikely]] b = dist(generator);

                std::swap(*(begin + b), *(begin + a));
                inject(b, a);
            }
        }
    }
}

#endif