/**
 * @file sort_wrapper_concepts.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 包装器以及concept
 * @version 5.0
 * @date 2026-05-08
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_ALGO_SORT_WP_CNT
#define ALIB5_ALGO_SORT_WP_CNT
#include <alib5/algorithm/sort_pos_iterator.h>

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
    
    template<class T> struct is_inject_pos_iterator : std::false_type {};
    template<class I, class F> struct is_inject_pos_iterator<InjectPosIterator<I, F>> : std::true_type {};
    template<class T> constexpr bool is_inject_pos_iterator_v = is_inject_pos_iterator<std::decay_t<T>>::value;

    template<class IterType, class Fn>
    auto make_inject_iterator(IterType it, Fn&& fn, size_t pos = 0) {
        if constexpr (is_inject_pos_iterator_v<IterType>) {
            return it;
        } else {
            return InjectPosIterator<IterType, std::decay_t<Fn>>(it, std::forward<Fn>(fn), pos);
        }
    }

    template<class T> concept IsRandomAccessIterator = 
        std::random_access_iterator<T> ||
        std::is_pointer_v<T>;

    template<class T> concept IsForwardIterator = requires(T& t){
        ++t;
        { t.next() } -> std::convertible_to<T>;
    };
    
    template<class T> concept IsBiDirectionalIterator = requires(T& t){
        ++t;
        --t;
        { t.next() } -> std::convertible_to<T>;
        { t.prev() } -> std::convertible_to<T>;
    };

    /// 支持随机访问和forward访问
    template<class T> concept IsGenericIterator = 
        IsBiDirectionalIterator<T> ||
        IsForwardIterator<T>       ||
        IsRandomAccessIterator<T>  ;
}
#endif