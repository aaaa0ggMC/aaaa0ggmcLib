/**
 * @file base.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 算法基础组件：带位置追踪的迭代器、比较器与注入器包装器、概念 (alib6.algo:base)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <iterator>
#include <concepts>
#include <functional>
#include <type_traits>
#include <cstddef>
#include <utility>

export module alib6.algo:base;

export namespace alib6::algo {

    template<class IterType, class Fn = std::nullptr_t>
    struct InjectPosIterator;

    /**
     * @brief 带位置计数的迭代器包装器
     */
    template<class IterType, class Fn>
    struct InjectPosIterator {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = typename std::iterator_traits<IterType>::iterator_category;
        using value_type        = typename std::iterator_traits<IterType>::value_type;
        using difference_type   = typename std::iterator_traits<IterType>::difference_type;
        using pointer           = typename std::iterator_traits<IterType>::pointer;
        using reference         = typename std::iterator_traits<IterType>::reference;

        IterType it{};
        std::size_t data{0};

        InjectPosIterator() = default;

        template<class R>
        InjectPosIterator(IterType it, R&&, std::size_t pos = 0)
            : it(it), data(pos) {}

        InjectPosIterator& operator++() requires requires(IterType it) { ++it; } { 
            ++it;
            ++data; 
            return *this;
        }

        InjectPosIterator& operator--() requires requires(IterType it) { --it; } { 
            --it;
            --data; 
            return *this;
        }

        InjectPosIterator operator++(int) requires requires(IterType it) { ++it; } {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        InjectPosIterator operator--(int) requires requires(IterType it) { --it; } {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        InjectPosIterator& operator+=(difference_type n) requires requires(IterType it, difference_type n) { it += n; } {
            it += n;
            data += n;
            return *this;
        }

        InjectPosIterator& operator-=(difference_type n) requires requires(IterType it, difference_type n) { it -= n; } {
            it -= n;
            data -= n;
            return *this;
        }

        InjectPosIterator operator+(difference_type n) const requires requires(IterType it, difference_type n) { it + n; } {
            auto tmp = *this;
            tmp += n;
            return tmp;
        }

        friend InjectPosIterator operator+(difference_type n, const InjectPosIterator& other) requires requires(IterType it, difference_type n) { it + n; } {
            return other + n;
        }

        InjectPosIterator operator-(difference_type n) const requires requires(IterType it, difference_type n) { it - n; } {
            auto tmp = *this;
            tmp -= n;
            return tmp;
        }

        difference_type operator-(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a - b; } {
            return it - other.it;
        }

        reference operator[](difference_type n) const requires requires(IterType it, difference_type n) { it[n]; } {
            return it[n];
        }
        
        [[nodiscard]] std::size_t get_index() const noexcept { 
            return data; 
        } 
        
        decltype(auto) operator*() const { return *it; }
        
        pointer operator->() const requires requires(IterType it) { it.operator->(); } || std::is_pointer_v<IterType> {
            if constexpr (std::is_pointer_v<IterType>) return it;
            else return it.operator->();
        }

        bool operator!=(const InjectPosIterator& other) const { return it != other.it; }
        bool operator==(const InjectPosIterator& other) const { return it == other.it; }
        bool operator<(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a < b; } { return it < other.it; }
        bool operator>(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a > b; } { return it > other.it; }
        bool operator<=(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a <= b; } { return it <= other.it; }
        bool operator>=(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a >= b; } { return it >= other.it; }
    };

    template<class IterType>
    struct InjectPosIterator<IterType, std::nullptr_t> {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = typename std::iterator_traits<IterType>::iterator_category;
        using value_type        = typename std::iterator_traits<IterType>::value_type;
        using difference_type   = typename std::iterator_traits<IterType>::difference_type;
        using pointer           = typename std::iterator_traits<IterType>::pointer;
        using reference         = typename std::iterator_traits<IterType>::reference;

        IterType it{};

        InjectPosIterator() = default;

        InjectPosIterator(IterType it, std::nullptr_t, std::size_t = 0)
            : it(it) {}

        InjectPosIterator& operator++() requires requires(IterType it) { ++it; } { 
            ++it;
            return *this;
        }

        InjectPosIterator& operator--() requires requires(IterType it) { --it; } { 
            --it;
            return *this;
        }

        InjectPosIterator operator++(int) requires requires(IterType it) { ++it; } {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        InjectPosIterator operator--(int) requires requires(IterType it) { --it; } {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        InjectPosIterator& operator+=(difference_type n) requires requires(IterType it, difference_type n) { it += n; } {
            it += n;
            return *this;
        }

        InjectPosIterator& operator-=(difference_type n) requires requires(IterType it, difference_type n) { it -= n; } {
            it -= n;
            return *this;
        }

        InjectPosIterator operator+(difference_type n) const requires requires(IterType it, difference_type n) { it + n; } {
            auto tmp = *this;
            tmp += n;
            return tmp;
        }

        friend InjectPosIterator operator+(difference_type n, const InjectPosIterator& other) requires requires(IterType it, difference_type n) { it + n; } {
            return other + n;
        }

        InjectPosIterator operator-(difference_type n) const requires requires(IterType it, difference_type n) { it - n; } {
            auto tmp = *this;
            tmp -= n;
            return tmp;
        }

        difference_type operator-(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a - b; } {
            return it - other.it;
        }

        reference operator[](difference_type n) const requires requires(IterType it, difference_type n) { it[n]; } {
            return it[n];
        }

        [[nodiscard]] std::size_t get_index() const noexcept { return 0; }     
        
        decltype(auto) operator*() const { return *it; }   
        
        pointer operator->() const requires requires(IterType it) { it.operator->(); } || std::is_pointer_v<IterType> {
            if constexpr (std::is_pointer_v<IterType>) return it;
            else return it.operator->();
        }
        
        bool operator!=(const InjectPosIterator& other) const { return it != other.it; }
        bool operator==(const InjectPosIterator& other) const { return it == other.it; }
        bool operator<(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a < b; } { return it < other.it; }
        bool operator>(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a > b; } { return it > other.it; }
        bool operator<=(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a <= b; } { return it <= other.it; }
        bool operator>=(const InjectPosIterator& other) const requires requires(IterType a, IterType b) { a >= b; } { return it >= other.it; }
    };

    template<class IterType, class R>
    InjectPosIterator(IterType, R&&, std::size_t = 0) -> InjectPosIterator<IterType, std::decay_t<R>>;

    template<class Fn> 
    auto wrap_compare(Fn&& fn) {
        return [f = std::forward<Fn>(fn)]<class Tp>(const Tp& a, const Tp& b, bool reverse) {
            return reverse ? f(b, a) : f(a, b);
        };
    }
    
    template<class Fn> 
    auto wrap_inject(Fn&& fn) {
        return [f = std::forward<Fn>(fn)](std::size_t prev, std::size_t aft) {
            if constexpr (!std::is_same_v<std::decay_t<Fn>, std::nullptr_t>) {
                f(prev, aft);
            }
        };
    }

    template<class Fn, class T> 
    concept IsCompareFn = requires(Fn&& fn, const T& a, const T& b) {
        { fn(a, b) } -> std::convertible_to<bool>;
    };

    template<class Fn> 
    concept IsInjectFn = requires(Fn&& fn, std::size_t prev, std::size_t aft) {
        fn(prev, aft);
    } || std::is_same_v<std::decay_t<Fn>, std::nullptr_t>;

    template<class T> struct is_inject_pos_iterator : std::false_type {};
    template<class I, class F> struct is_inject_pos_iterator<InjectPosIterator<I, F>> : std::true_type {};
    template<class T> constexpr bool is_inject_pos_iterator_v = is_inject_pos_iterator<std::decay_t<T>>::value;

    template<class IterType, class Fn>
    auto make_inject_iterator(IterType it, Fn&& fn, std::size_t pos = 0) {
        if constexpr (is_inject_pos_iterator_v<IterType>) {
            return it;
        } else {
            return InjectPosIterator<IterType, std::decay_t<Fn>>(it, std::forward<Fn>(fn), pos);
        }
    }

    template<class T> 
    concept IsRandomAccessIterator = std::random_access_iterator<T> || std::is_pointer_v<T>;

    template<class T> 
    concept IsForwardIterator = std::forward_iterator<T>;
    
    template<class T> 
    concept IsBiDirectionalIterator = std::bidirectional_iterator<T>;

    template<class T> 
    concept IsGenericIterator = IsBiDirectionalIterator<T> || IsForwardIterator<T> || IsRandomAccessIterator<T>;

    constexpr std::size_t search_npos = std::numeric_limits<std::size_t>::max();

    template<class T1, class T2> 
    concept IsComparableIterators = requires(T1& a, T2& b) {
        { (*a) != (*b) } -> std::convertible_to<bool>;
    };

} // namespace alib6::algo
