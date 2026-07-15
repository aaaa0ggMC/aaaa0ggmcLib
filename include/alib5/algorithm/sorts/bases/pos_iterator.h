/**
 * @file pos_iterator.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Iterator wrapper supporting position counting / 支持计数的迭代器
 * @version 5.0
 * @date 2026-05-08
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_ALGO_SORT_POS_IT
#define ALIB5_ALGO_SORT_POS_IT

#include <iterator>
#include <type_traits>
#include <cstddef>

namespace alib5::algo::sort {

    /**
     * @brief An iterator wrapper that injects position counting capabilities.
     * 
     * @tparam IterType The underlying iterator type.
     * @tparam Fn The injection function type.
     */
    template<class IterType, class Fn> 
    struct InjectPosIterator {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = typename std::iterator_traits<IterType>::iterator_category;
        using value_type        = typename std::iterator_traits<IterType>::value_type;
        using difference_type   = typename std::iterator_traits<IterType>::difference_type;
        using pointer           = typename std::iterator_traits<IterType>::pointer;
        using reference         = typename std::iterator_traits<IterType>::reference;

        IterType it {};
        size_t data { 0 };

        InjectPosIterator() = default;

        template<class R>
        InjectPosIterator(IterType it, R&&, size_t pos = 0)
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
        
        size_t get_index() const { 
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

    /**
     * @brief Specialized template for InjectPosIterator when no injection is applied (using std::nullptr_t).
     * 
     * @tparam IterType The underlying iterator type.
     */
    template<class IterType>
    struct InjectPosIterator<IterType, std::nullptr_t> {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = typename std::iterator_traits<IterType>::iterator_category;
        using value_type        = typename std::iterator_traits<IterType>::value_type;
        using difference_type   = typename std::iterator_traits<IterType>::difference_type;
        using pointer           = typename std::iterator_traits<IterType>::pointer;
        using reference         = typename std::iterator_traits<IterType>::reference;

        IterType it {};

        InjectPosIterator() = default;

        InjectPosIterator(IterType it, std::nullptr_t, size_t = 0)
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

        size_t get_index() const { return 0; }     
        
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

    /**
     * @par Notes / 备注:
     * Deduction guide to ensure `nullptr` resolves directly to `std::nullptr_t`.
     * / 推导指导，让nullptr就是nullptr_t
     */
    template<class IterType, class R>
    InjectPosIterator(IterType, R&&, size_t = 0) -> InjectPosIterator<IterType, std::decay_t<R>>;

} // namespace alib5::algo::sort

#endif // ALIB5_ALGO_SORT_POS_IT