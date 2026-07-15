/**
 * @file quick.inl
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 快速排序
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_QUICK
#define ALIB5_AALGO_SORT_QUICK
#include <alib5/algorithm/sorts/base.h>
#include <vector>

namespace alib5::algo::sort{
     /**
     * @brief Quick sort Lomuto partitioning.
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * Fast and slow pointer approach. Initially, both pointers start at the front. `cur` continuously checks if it's greater than `high`.
     * If `cur` is less than `high`, place `cur` at the front and move `i` forward by one slot.
     * Finally, swap `i` and `high`.
     * Now, everything to the left of `high` is smaller, and to the right is larger.
     * / 快慢指针处理,开始两个指针都在最前面,cur不断往下检查是否大于high
     * 如果 cur小于high那么此时把cur放到前方,i往前面移动一个格子
     * 最后执行完毕时交换i和high
     * 此时 high 数据的左侧都是比high小的,右侧都是比high大的
     * @endcode
     */
    struct PartitionLomuto {
        template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node> 
        static void partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end);
    };

    /**
     * @brief Quick sort Hoare partitioning.
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * First, establish three positions: `low`, `high`, and a simple `pivot` median.
     * Both cursors move towards the pivot, ensuring elements before `low'` are smaller and after `high'` are larger.
     * Then swap `low'` and `high'` to maintain order properties until they collide (finishing the partition).
     * The collision point defines the partition.
     * Theoretically, duplicate elements (copies) can appear, which is risky for unknown types, so pivot tracking is injected here.
     * / 先找三个位置low high 以及简单的中位数 pivot
     * 两边游标不断往pivot,那么就保证了low' 前面的都小于pivot, high' 后面的都大于pivot
     * 那么此时便尝试对low' high'交换,这样就保证了有序性质
     * 直到l h碰撞说明分完区了
     * 此时按照碰撞位置进行分区
     * 理论上来讲普通hoare排序运行过程中会出现重复元素,也就是copy
     * 这在不知道数据类型的情况下还是比较难绷的,因此这里加入了对pivot的追踪
     * @endcode
     */
    struct PartitionHoare {
        template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end);
    };

    /**
     * @brief Median-of-three partitioning algorithm.
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * A simple improvement to Hoare's algorithm. It ensures the median order on both left and right sides to facilitate partitioning.
     * / 简单改良的hoare算法,保证了左侧右侧中位数的顺序性,方便分区
     * @endcode
     */
    struct PartitionMedianThree {
        template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end);
    };

    /**
     * @brief [R] Basic quick sort. Time complexity O(n log n). Tier: B
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * Continually partition to ensure elements in the left partition are always smaller than those in the right.
     * Recursively apply this until partitions have only two elements, then perform a simple swap.
     * Details are handled in the partition method.
     * / 通过不断遍历分区保证左边分区的内容始终小于右边的内容
     * 之后递归直到只有两个元素,此时简单swap就行
     * 具体的处理见partition的分区方式
     * @endcode
     */
    template<
        class PartitionMethod = PartitionMedianThree, IsRandomAccessIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
    > 
    void quick(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr){
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
}

namespace alib5::algo::sort{
    template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node> 
    void PartitionLomuto::partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end) {
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

    template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node>
    void PartitionHoare::partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end) {
        using value_t = typename std::iterator_traits<IterType>::value_type;
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

    template<IsRandomAccessIterator IterType, class CompareFn, class InjectFn, class Node>
    void PartitionMedianThree::partition(IterType low, IterType high, CompareFn& compare, bool reverse, InjectFn& inject, std::vector<Node>& nodes, IterType begin, IterType end) {
        IterType pivot = (low + (high - low) / 2);

        bool c1 = compare(*low, *pivot, reverse);
        bool c2 = compare(*pivot, *high, reverse);
    
        if (!(c1 && c2)) {
            bool c3 = compare(*low, *high, reverse);
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
}

#endif