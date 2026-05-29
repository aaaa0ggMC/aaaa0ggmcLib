/**
 * @file adiscrete_math.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 只是离散数学编程实践罢了，注意，测评机器似乎为C++14甚至C++11，因此没有string_view
 * @version 5.0
 * @date 2026-05-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_DISCRETE_MATH_H
#define ALIB5_DISCRETE_MATH_H
#include <type_traits>
#include <string_view>
#include <vector>
#include <cmath>

namespace alib5{

    template<class T> requires std::is_integral_v<T>
    bool congruence_modulo(const T & m,const T & a,const T & b){
        return (a % m) == (b % m);
    }

    template<class T,class MT> 
#ifdef __cpp_lib_concepts
    requires std::is_convertible_v< T, std::string_view> && std::is_integral_v<MT>
#endif
    bool congruence_modulo(const MT & m,const T & sa,const T & sb){
        std::string_view a = sa;
        std::string_view b = sb;

        MT remainder_a {};
        MT remainder_b {};

        for(auto ch : a){
            remainder_a = (remainder_a * 10 + (ch - '0'));
            if(remainder_a >= m) remainder_a %= m;
        }

        for(auto ch : b){
            remainder_b = (remainder_b * 10 + (ch - '0'));
            if(remainder_b >= m) remainder_b %= m;
        }
    
        return remainder_a == remainder_b;
    }

    template<class T>
#ifdef __cpp_lib_concepts
    requires std::is_integral_v<T>
#endif
    T fast_pow_modulo(T base,T power,T modulo){
        T result = 1 % modulo;
        base %= modulo;
        while(power > 0){
            if(power & 1)
                result = (result * base) % modulo;
            base = (base * base) % modulo;
            power >>= 1;
        }
        return result;
    }

    long long isqrt(long long n) {
        if (n <= 1) return n;
        long long x = n;
        long long y = (x + n / x) >> 1;
        while (y < x) {
            x = y;
            y = (x + n / x) >> 1;
        }
        return x;
    }
    
    template<class T>
#ifdef __cpp_lib_concepts
    requires std::is_integral_v<T>
#endif
    bool is_prime(T n){
        if(n == 0)return false;
        else if(n == 1)return false;
        else if(n == 2)return true;

        T boundary = std::isqrt(n);
        if(boundary < 2)boundary = 2;
        for(T a = 2;a <= boundary;++a){
            if(n % a == 0)return false;
        }
        return true;
    }

    struct SplitInfo{
        long long base {};
        long long power {};
    };

    template<class T>
#ifdef __cpp_lib_concepts
    requires std::is_integral_v<T>
#endif
    std::vector<SplitInfo> split_number(T n){
        std::vector<SplitInfo> info;

        auto deal = [&](T base){
            T pow = 0;
            while(n % base == 0){
                ++pow;
                n /= base;
            }
            return pow;
        };
        if(n % 2 == 0){
            info.emplace_back(
                2,
                deal(2)
            );
        }

        for(size_t i = 3;i <= n;i += 2){
            if(n % i == 0 && is_prime(i)){
                info.emplace_back(
                    i,
                    deal(i)
                );
            }
        }

        return info;
    }

};


#endif