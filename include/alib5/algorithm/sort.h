/**
 * @file sort.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Various sorting algorithms. When reverse=false and default compare is used, it guarantees ascending order. Check docs/aalgorithm.md for test scenarios. 
 * / 各种排序算法，在reverse=false的情况下，若使用defcompare，即arg1 < arg2,保证升序 \ 每个算法是怎么测试的可以看docs/aalgorithm.md查看情况
 * @version 5.0
 * @date 2026/06/10
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/12/17 
*/
#ifndef ALIB5_ALGO_SORT_H_INCLUDED
#define ALIB5_ALGO_SORT_H_INCLUDED

#include <alib5/algorithm/sort_wrapper_concepts.h>
#include <utility>
#include <iterator>
#include <vector>

#ifndef ALIB5_ALGO_DISABLE_ENTERTAIN
#include <random>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#endif

// =========================================================================
// API Declarations (Forward Declarations) / API 声明区
// =========================================================================
namespace alib5::algo::sort {

    ////// RANDOM ACCESS ITERATOR ONLY //////

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
    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void insertion(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    enum class ShellGapType { Div2, Knuth, Hibbard };

    /**
     * @brief [R] Simple shell sort. Time complexity O(n^1.3) - O(n^2). Tier: D
     * 
     * @par Notes / 备注:
     * Basically insertion sort with a gap. / 就是带上gap的插入排序
     */
    template<ShellGapType GapType = ShellGapType::Div2,
             IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void shell(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    /**
     * @brief [R] Gnome sort.
     * 
     * @par Algorithm Rules / 算法规律记录
     * If a > b, step forward. If a < b, swap and step backward.
     * / 树精排序 a > b 往前走, a < b 交换,往后走
     */
    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void gnome(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    /**
     * @brief [R] Comb sort. Bubble sort improvement.
     * 
     * @par Algorithm Rules / 算法规律记录
     * A Bubble sort improvement utilizing a fixed shrink gap factor of 1.247.
     * / 依旧冒泡改良,梳排序. 选择了一个固定步长比例 1.247
     */
    template<float GapFactor = 1.247f,
             IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void comb(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    /**
     * @brief [R] Cocktail shaker sort.
     * 
     * @par Algorithm Rules / 算法规律记录
     * Bidirectional bubble sort: bubbles left, then right.
     * Simple optimization using a swappable flag. If no swap occurs, sorting is complete.
     * / 鸡尾酒排序,左边冒泡一次右边冒泡一次. 简单优化:swappable,没有swappable便说明排序完毕了
     */
    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void cocktail(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


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
    template<class PartitionMethod = PartitionMedianThree, IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void quick(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    //////// GENERIC ITERATOR ALLOWED ///////

    /**
     * @brief [RFD] Simple selection sort. Time complexity O(n^2). Tier: E
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * a b c d e f g
     * Start from 'a' and move forward, finding the minimum subsequent element and putting it at the front.
     * / 从a开始往后走,每次把后面最小的放前面,然后就没了
     * @endcode
     */
    template<IsGenericIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void selection(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    /**
     * @brief [R] Basic bubble sort. Time complexity O(n^2). Tier: E
     * 
     * @par Algorithm Rules / 算法规律记录
     * @code
     * a b c d e f g
     * Starting from index 0, if a > next, swap. Do this (n - (index + 1)) times for (n - 1) rounds.
     * Optimization: use a `swapped` flag to exit early if no swaps are made.
     * / 从0号位开始如果 a > 下一个就交换持续,执行(n-(index+1))(index从0开始)次,执行(n-1)轮次
     * 其他的类似
     * 可引入优化 swapped,如果swapped为false就可以提前跑了
     * For example, sorting 3 2 1: / 比如 3 2 1 排序
     * 3 > 2 swap: 2 3 1
     * 3 > 1 swap: 2 1 3
     * 2 > 1 swap: 1 2 3
     * It has executed (n-2) times here, stop swapping for this iteration. 
     * Loop has run (n-1) rounds. Exit.
     * / 此时执行了(n-2)次了,因此不要进行交换了
     * / 已经执行(n-1)轮.离开
     * @endcode
     */
    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void bubble(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    /**
     * @brief [R] Odd-even sort (bubble sort variation).
     */
    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void odd_even(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);


    //////// ENTERTAIN ONLY ////////
#ifndef ALIB5_ALGO_DISABLE_ENTERTAIN

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
    template<size_t SwapsBetweenChecks = 31,
             IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn = std::nullptr_t> 
    void bozo(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse = false, InjectFn&& i_inject = nullptr);
    
    /**
     * @brief [R] Sleep sort. Literally uses actual system thread sleeping.
     * / 睡眠排序,真的用的是系统睡眠
     */
    template<class TimerBasic = std::chrono::milliseconds,
             size_t MultiplyBase = 1,
             IsRandomAccessIterator IterType> 
    void sleep(IterType begin, IterType end, bool reverse = false);

#endif

} // namespace alib5::algo::sort


// =========================================================================
// Implementation Details / 具体实现区
// =========================================================================
namespace alib5::algo::sort {

    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void insertion(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
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

    template<ShellGapType GapType,
             IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void shell(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;

        size_t len = std::distance(begin, end);
        size_t gap = 1;

        if constexpr(GapType == ShellGapType::Div2) {
            gap = len / 2;
        } else if constexpr(GapType == ShellGapType::Knuth) {
            while (gap < len / 3) gap = 3 * gap + 1;
        } else if constexpr(GapType == ShellGapType::Hibbard) {
            while (gap <= len) gap = (gap << 1) | 1;
            gap >>= 2;
            if (gap == 0) gap = 1;
        }

        auto gap_fn = [&]() {
            if constexpr(GapType == ShellGapType::Div2) {
                gap >>= 1;
            } else if constexpr(GapType == ShellGapType::Knuth) {
                gap = gap / 3;
            } else if constexpr(GapType == ShellGapType::Hibbard) {
                gap >>= 1;
            }
        };

        while (gap > 0) {
            for (size_t begin_pos = 0; begin_pos < gap; ++begin_pos) {
                for (size_t c_begin = begin_pos + gap; c_begin < len; c_begin += gap) {
                    auto s_begin = begin + c_begin;
                    auto j = s_begin;
                    value_t val = std::move(*s_begin);

                    while (j > (begin + begin_pos) && compare(val, *(j - gap), reverse)) {
                        *j = std::move(*(j - gap));
                        inject((j - gap).get_index(), j.get_index());
                        j -= gap;
                    }
                    *j = std::move(val);
                }
            }
            gap_fn();
        }
    }

    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void gnome(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
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

    template<float GapFactor,
             IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void comb(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
        static_assert(GapFactor > 1, "Gap factor is required to be bigger than 1!");
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        size_t len = std::distance(begin, end);
        size_t gap = len;

        bool swapped = true;
        while (gap > 1 || swapped) {
            swapped = false;
            gap /= GapFactor;
            if (gap < 1) gap = 1;

            for (auto i = begin; i < end - gap; ++i) {
                if (compare(*(i + gap), *i, reverse)) {
                    std::swap(*i, *(i + gap));
                    inject(i.get_index(), (i + gap).get_index());
                    swapped = true;
                }
            }
        }
    }

    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void cocktail(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool swapped = true;
    
        auto high_sorted = end;
        auto low_sorted = begin;
        while (low_sorted < high_sorted && swapped) {
            swapped = false;

            for (auto i_current = low_sorted; i_current < high_sorted - 1; ++i_current) {
                if (compare(*(i_current + 1), *i_current, reverse)) {
                    std::swap(*i_current, *(i_current + 1));
                    inject(i_current.get_index(), (i_current + 1).get_index());
                    swapped = true;
                }
            }
            --high_sorted;
            
            if (!swapped) [[unlikely]] break;

            for (auto i_current = high_sorted - 1; i_current > low_sorted; --i_current) {
                if (compare(*i_current, *(i_current - 1), reverse)) {
                    std::swap(*i_current, *(i_current - 1));
                    inject(i_current.get_index(), (i_current - 1).get_index());
                    swapped = true;
                }
            }
            ++low_sorted;
        }
    }

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

    template<class PartitionMethod, IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void quick(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
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

    template<IsGenericIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void selection(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;

        for (auto current = begin; current != end; ++current) {
            auto front = current;
            for (auto inner = current + 1; inner != end; ++inner) {
                if (compare(*inner, *front, reverse)) {
                    front = inner;
                }
            }
            if (front != current) {
                std::swap(*front, *current);
                inject(front.get_index(), current.get_index());
            }
        }
    }

    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void bubble(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool swapped = true;
    
        for (auto current = begin; current < (end - 1) && swapped; ++current) {
            swapped = false;
            for (auto i_current = begin; i_current < (end - 1 - (current.get_index())); ++i_current) {
                if (compare(*(i_current + 1), *i_current, reverse)) {
                    std::swap(*i_current, *(i_current + 1));
                    inject(i_current.get_index(), (i_current + 1).get_index());
                    swapped = true;
                }
            }
        }
    }

    template<IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void odd_even(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));

        auto begin = make_inject_iterator(i_begin, i_inject, 0);
        auto end = make_inject_iterator(i_end, i_inject, std::distance(i_begin, i_end));

        if (begin == end) return;
        bool swapped = true;
    
        while (swapped) {
            swapped = false;
            for (auto i = begin; i < end - 1; i += 2) {
                if (compare(*(i + 1), *i, reverse)) {
                    std::swap(*(i + 1), *i);
                    inject((i + 1).get_index(), i.get_index());
                    swapped = true;
                }
            }
            for (auto i = begin + 1; i < end - 1; i += 2) {
                if (compare(*(i + 1), *i, reverse)) {
                    std::swap(*(i + 1), *i);
                    inject((i + 1).get_index(), i.get_index());
                    swapped = true;
                }
            }
        }
    }

#ifndef ALIB5_ALGO_DISABLE_ENTERTAIN
    template<size_t SwapsBetweenChecks,
             IsRandomAccessIterator IterType,
             IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
             IsInjectFn InjectFn> 
    void bozo(IterType i_begin, IterType i_end, CompareFn&& i_compare, bool reverse, InjectFn&& i_inject) {
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
    
    template<class TimerBasic, size_t MultiplyBase, IsRandomAccessIterator IterType> 
    void sleep(IterType begin, IterType end, bool reverse) {
        static_assert(requires{ (size_t)(*begin); }, "Data must be able to be casted to double!");
        if (begin == end) return;

        std::mutex mtx;
        bool ready = false;
        std::condition_variable cv;
        std::vector<std::jthread> threads;
        IterType fill = reverse ? end - 1 : begin;

        for (IterType a = begin; a != end; ++a) {
            threads.emplace_back([reverse, &fill, time = (size_t)(*a), data = std::move(*a), &ready, &cv, &mtx] {
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [&ready] { return ready; });
                }
                std::this_thread::sleep_for(TimerBasic((size_t)time * MultiplyBase));
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    *fill = std::move(data);
                    if (reverse) --fill;
                    else ++fill;
                }
            });
        }
        {
            std::unique_lock<std::mutex> lock(mtx);
            ready = true;
            cv.notify_all();
        }
    }
#endif

} // namespace alib5::algo::sort

#endif // ALIB5_ALGO_SORT_H_INCLUDED