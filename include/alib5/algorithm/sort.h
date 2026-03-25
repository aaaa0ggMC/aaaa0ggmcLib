/**
 * @file sort.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 各种排序算法，在reverse=false的情况下，若使用defcompare，即arg1 < arg2,保证升序 \ 
 * 每个算法是怎么测试的可以看docs/aalgorithm.md查看情况
 * @version 5.0
 * @date 2026/03/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/12/17 
*/
#ifndef ALIB5_ALGO_SORT_H_INCLUDED
#define ALIB5_ALGO_SORT_H_INCLUDED
#include <utility>
#include <iterator>
#include <vector>

namespace alib5::algo::sort{
    template<class T> using default_compare = std::less<T>;
    template<class Fn> auto wrap_compare(Fn && fn){
        return [f = std::forward<decltype(fn)>(fn)]<class Tp>(const Tp & a,const Tp & b,bool reverse){
            return reverse ? f(b,a) : f(a,b);
        };
    }

    template<class Fn> auto wrap_inject(Fn && fn){
        return [f = std::forward<decltype(fn)>(fn)](std::size_t prev,std::size_t aft){
            if constexpr(!std::is_same_v<std::decay_t<decltype(f)>,std::nullptr_t>){
                f(prev,aft);
            }
        };
    }
    
    template<class Fn,class T> concept IsCompareFn = 
        requires(Fn && fn,const T& a,const T& b,bool & m){
            { fn(a,b) } -> std::convertible_to<bool>;
        };
    template<class Fn> concept IsInjectFn = 
        requires(Fn && fn,std::size_t swap_prev,std::size_t swap_aft){
            fn(swap_prev,swap_aft);
        } || 
        requires(){
            std::is_same_v<std::decay_t<Fn>,std::nullptr_t>;
        };
    template<class T> concept IsIterator = 
        std::random_access_iterator<T> ||
        std::is_pointer_v<T>;

    /// @brief 基础的插入排序，时间复杂度O(n^2)   Tier: D
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    /// @par 算法规律记录
    /// @code
    /// a b c d e f g
    /// (从a b开始),因为a开始时a已经是排完序了的
    ///     此时把b看成空区域,比较a b,因为a已经排序完毕,因此往后推移是b该插入的区域不断往前,直到遇到和b比较为false的
    /// 比如
    /// 3 2 1 排序
    /// 3 2 因为 3 > 2因此3放到2处,有结果为 2 3,此时结束
    /// 2 3 1 因为 3 > 1 因此插入1的位置往前,有 2 1 3
    /// 2 1 3 因为 2 > 1 因此插入1位置往前,有1 2 3
    /// @endcode 
    template<IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
            > 
    void insertion(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;

        for(IterType current = (begin + 1);current < end;++current){
            value_t val = std::move(*current);
            IterType j = current;

            while(j > begin && compare(val,*(j-1),reverse)){
                *j = std::move(*(j-1));
                inject(std::distance(begin,j),std::distance(begin,j-1));
                --j;
            }
            *j = std::move(val);
        }
    }

    enum class ShellGapType{
        Div2,
        Knuth,
        Hibbard
    };

    /// @brief 简单的希尔排序 时间复杂度O(n^1.3) - O(n^2) Tier:D 
    /// 就是带上gap的插入排序
    template<ShellGapType GapType = ShellGapType::Div2,
            IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
            > 
    void shell(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;

        size_t len = std::distance(begin,end);
        size_t gap = 1;

        if constexpr(GapType == ShellGapType::Div2){
            gap = len / 2;
        }else if constexpr(GapType == ShellGapType::Knuth){
            while(gap < len / 3)gap = 3 * gap + 1;
        }else if constexpr(GapType == ShellGapType::Hibbard){
            while(gap <= len) gap = (gap << 1) | 1;
            gap >>= 2;
            if(gap == 0)gap = 1;
        }

        auto gap_fn = [&](){
            if constexpr(GapType == ShellGapType::Div2) {
                gap >>= 1;
            }else if constexpr(GapType == ShellGapType::Knuth){
                gap = gap / 3;
            }else if constexpr(GapType == ShellGapType::Hibbard){
                gap >>= 1;
            }
        };

        while(gap > 0){
            for(size_t begin_pos = 0;begin_pos < gap;++begin_pos){
                for(size_t c_begin = begin_pos + gap;c_begin < len;c_begin += gap){
                    IterType s_begin = begin + c_begin;
                    IterType j = s_begin;
                    value_t val = std::move(*s_begin);

                    while(j > (begin + begin_pos) && compare(val,*(j - gap),reverse)){
                        *j = std::move(*(j - gap));
                        inject(std::distance(begin,j - gap),std::distance(begin,j));
                        j -= gap;
                    }
                    *j = std::move(val);
                }
            }
            gap_fn();
        }
    }

    /// @brief 简单的选择排序 O(n^2) Tier:E
    /// @code
    /// a b c d e f g
    /// 从a开始往后走,每次把后面最小的放前面,然后就没了
    /// @endcode 
    template<IsIterator IterType,
        IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
        IsInjectFn InjectFn = std::nullptr_t
        > 
    void selection(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;

        for(IterType current = begin;current != end;++current){
            IterType front = current;
            for(IterType inner = current + 1;inner != end;++inner){
                if(compare(*inner,*front,reverse)){
                    front = inner;
                }
            }
            if(front != current){
                std::swap(*front,*current);
                inject(std::distance(begin,front),std::distance(begin,current));
            }
        }
    }
    
    /// @brief 基础的冒泡排序，时间复杂度O(n^2) Tier:E
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作    /// @par 算法规律记录
    /// @code
    /// a b c d e f g
    /// 从0号位开始如果 a > 下一个就交换持续,执行(n-(index+1))(index从0开始)次,执行(n-1)轮次
    /// 其他的类似
    /// 可引入优化 swapped,如果swapped为false就可以提前跑了
    /// 比如
    /// 3 2 1 排序
    /// 3 > 2 交换 2 3 1
    /// 3 > 1 交换 2 1 3
    /// 2 > 1 交换 1 2 3
    /// 此时执行了(n-2)次了,因此不要进行交换了
    /// 已经执行(n-1)轮.离开
    /// @endcode 
    template<IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type>  CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
        > 
    void bubble(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;
        bool swapped = true;
    
        for(IterType current = begin;current < (end - 1) && swapped;++current){
            swapped = false;
            for(IterType i_current = begin;i_current < (end - 1 - (current - begin));++i_current){
                if(compare(*(i_current+1),*i_current,reverse)){
                    std::swap(*i_current,*(i_current+1));
                    inject(std::distance(begin,i_current),std::distance(begin,i_current + 1));
                    swapped = true;
                }
            }
        }
    }

    /// @brief 快排Lomuto分区方式
    /// @par 算法规律记录
    /// @code
    /// 快慢指针处理,开始两个指针都在最前面,cur不断往下检查是否大于high
    /// 如果 cur小于high那么此时把cur放到前方,i往前面移动一个格子
    /// 最后执行完毕时交换i和high
    /// 此时 high 数据的左侧都是比high小的,右侧都是比high大的
    /// @endcode 
    struct PartitionLomuto {
        template<IsIterator IterType,class CompareFn,class InjectFn,class Node> 
            static void partition(IterType low,IterType high,
                CompareFn& compare,bool reverse,InjectFn & inject,
                std::vector<Node> & nodes,IterType begin,IterType end
            ){
                IterType i = low;

                for(IterType cur = low;cur < high;++cur){
                    if(compare(*cur,*high,reverse)){
                        if(i != cur){
                            std::swap(*i,*cur);
                            inject(std::distance(begin,i),std::distance(begin,cur));
                        }
                        ++i;
                    }
                }
                std::swap(*i,*high);
                inject(std::distance(begin,i),std::distance(begin,high));
                
                if(i > (low+1)){
                    nodes.emplace_back(low,i-1);
                }
                if(i < (high - 1)){
                    nodes.emplace_back(i+1,high);
                }
            }
    };

    /// @brief 快排Hoare分区方式
    /// @par 算法规律记录
    /// @code
    /// 先找三个位置low high 以及简单的中位数 pivot
    /// 两边游标不断往pivot,那么就保证了low' 前面的都小于pivot, high' 后面的都大于pivot
    /// 那么此时便尝试对low' high'交换,这样就保证了有序性质
    /// 直到l h碰撞说明分完区了
    /// 此时按照碰撞位置进行分区
    /// 理论上来讲普通hoare排序运行过程中会出现重复元素,也就是copy
    /// 这在不知道数据类型的情况下还是比较难绷的,因此这里加入了对pivot的追踪
    /// @endcode 
    struct PartitionHoare {
        template<IsIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high,
                            CompareFn& compare, bool reverse, InjectFn& inject,
                            std::vector<Node>& nodes,IterType begin,IterType end
        ){
            using value_t = typename std::iterator_traits<IterType>::value_type;
            IterType pivot = (low + (high - low) / 2);

            IterType l = low;
            IterType h = high;

            while(l <= h){
                while(compare(*l,*pivot,reverse)) ++l;
                while(compare(*pivot,*h,reverse)) --h;
                
                if(l <= h){
                    if(l != h){
                        [[unlikely]] if(l == pivot){
                            pivot = h;
                        }else [[unlikely]] if(h == pivot){
                            pivot = l;
                        }

                        std::swap(*l, *h);
                        inject(std::distance(begin,l),std::distance(begin,h));
                    }
                    ++l;
                    --h;
                }
            }

            if(low < h){
                nodes.emplace_back(low, h);
            }
            if(l < high){
                nodes.emplace_back(l, high);
            }
        }
    };

    /// @brief 三数取中算法
    /// @par 算法规律记录
    /// @code
    /// 简单改良的hoare算法,保证了左侧右侧中位数的顺序性,方便分区
    /// @endcode 
    struct PartitionMedianThree {
        template<IsIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high,
                            CompareFn& compare, bool reverse, InjectFn& inject,
                            std::vector<Node>& nodes,IterType begin,IterType end
        ){
            IterType pivot = (low + (high - low) / 2);

            // pivot > low ?
            bool c1 = compare(*low,*pivot,reverse);
            // pivot < high ?
            bool c2 = compare(*pivot,*high,reverse);
        
            if(!(c1 && c2)){
                // low < high
                bool c3 = compare(*low,*high,reverse);
                if(!c1 && c2){
                    std::swap(*low,*pivot);
                    inject(std::distance(begin,low),std::distance(begin,pivot));
                }else if(c1 && !c2){
                    std::swap(*high,*pivot);
                    inject(std::distance(begin,high),std::distance(begin,pivot));
                }
            }

            PartitionHoare::partition(low,high,compare,
                reverse,inject,nodes,begin,end);
        }
    };

    /// @brief 基础的快速排序，时间复杂度O(nlogn) Tier:B
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    /// @par 算法规律记录
    /// @code
    /// 通过不断遍历分区保证左边分区的内容始终小于右边的内容
    /// 之后递归直到只有两个元素,此时简单swap就行
    /// 具体的处理见partition的分区方式
    /// @endcode 
    template<class PartitionMethod = PartitionMedianThree,IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
    > 
    void quick(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;
    
        // 这里手动实现一个简单的stack
        struct Node{
            IterType low;
            IterType high;

            Node(IterType l,IterType h):low{l},high{h}{}
        };

        std::vector<Node> stack;
        stack.reserve(256);

        stack.emplace_back(begin,end-1);
        while(!stack.empty()){
            // partition
            Node poped = stack.back();
            stack.pop_back();

            IterType low = poped.low;
            IterType high = poped.high;

            PartitionMethod::partition(low,high,compare,
                        reverse,inject,stack,begin,end);
        }
    }
}

 #endif