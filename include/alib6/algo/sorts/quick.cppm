/**
 * @file quick.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 快速排序与分区策略 (alib6.algo:sort.quick)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <iterator>
#include <vector>
#include <utility>

export module alib6.algo:sort.quick;

import :base;

export namespace alib6::algo {

    struct PartitionLomuto {
        template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node> 
        static void partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end) {
            IterType i = low;

            for (IterType cur = low; cur < high; ++cur) {
                if (compare(*cur, *high, reverse)) {
                    if (i != cur) {
                        std::swap(*i, *cur);
                        inject(i.get_index(), cur.get_index());
                    }
                    ++i;
                }
            }
            std::swap(*i, *high);
            inject(i.get_index(), high.get_index());
            
            if (i > (low + 1)) {
                nodes.emplace_back(low, i - 1);
            }
            if (i < (high - 1)) {
                nodes.emplace_back(i + 1, high);
            }
        }
    };

    struct PartitionHoare {
        template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end) {
            IterType pivot = (low + (high - low) / 2);

            IterType l = low;
            IterType h = high;

            while (l <= h) {
                while (compare(*l, *pivot, reverse)) ++l;
                while (compare(*pivot, *h, reverse)) --h;
                
                if (l <= h) {
                    if (l != h) {
                        [[unlikely]] if (l == pivot) {
                            pivot = h;
                        } else [[unlikely]] if (h == pivot) {
                            pivot = l;
                        }

                        std::swap(*l, *h);
                        inject(l.get_index(), h.get_index());
                    }
                    ++l;
                    --h;
                }
            }

            if (low < h) {
                nodes.emplace_back(low, h);
            }
            if (l < high) {
                nodes.emplace_back(l, high);
            }
        }
    };

    struct PartitionMedianThree {
        template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end) {
            IterType pivot = (low + (high - low) / 2);

            bool c1 = compare(*low, *pivot, reverse);
            bool c2 = compare(*pivot, *high, reverse);
        
            if (!(c1 && c2)) {
                if (!c1 && c2) {
                    std::swap(*low, *pivot);
                    inject(low.get_index(), pivot.get_index());
                } else if (c1 && !c2) {
                    std::swap(*high, *pivot);
                    inject(high.get_index(), pivot.get_index());
                }
            }

            PartitionHoare::partition(low, high, compare, reverse, inject, nodes, begin, end);
        }
    };

    template<
        class PartitionMethod = PartitionMedianThree,
        IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void quick_sort(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        
        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
    
        struct Node {
            decltype(begin) low;
            decltype(begin) high;
            Node(decltype(begin) l, decltype(begin) h) : low{l}, high{h} {}
        };

        std::vector<Node> stack;
        stack.reserve(256);

        stack.emplace_back(begin, end - 1);
        while (!stack.empty()) {
            Node poped = stack.back();
            stack.pop_back();

            auto low = poped.low;
            auto high = poped.high;

            PartitionMethod::partition(low, high, compare, reverse, inject, stack, begin, end);
        }
    }

    template<
        class PartitionMethod = PartitionMedianThree,
        IsRandomAccessIterator IterType,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void quick_sort(IterType i_begin, IterType i_end, bool reverse = false, InjectFn&& i_inject = nullptr) {
        quick_sort<PartitionMethod>(i_begin, i_end, std::less<typename std::iterator_traits<IterType>::value_type>{}, reverse, std::forward<InjectFn>(i_inject));
    }

    template<class Range, class Comp = std::less<>, class Inject = std::nullptr_t>
    void quick_sort(Range& r, Comp comp = {}, bool reverse = false, Inject inject = nullptr) {
        quick_sort(std::begin(r), std::end(r), std::move(comp), reverse, std::move(inject));
    }

} // namespace alib6::algo
