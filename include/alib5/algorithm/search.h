/**
 * @file search.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 搜索算法?
 * @version 5.0
 * @date 2026-04-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef ALIB5_ALGO_SEARCH_H_INCLUDED
#define ALIB5_ALGO_SEARCH_H_INCLUDED
#include <iterator>
#include <limits>
#include <vector>

namespace alib5::algo::search{
    template<class T> concept IsIterator = 
    std::random_access_iterator<T> ||
    std::is_pointer_v<T>;
    
    constexpr size_t search_npos = std::numeric_limits<size_t>::max();

    /// 朴素的模式匹配
    template<IsIterator T1,IsIterator T2,bool safe_mode = true >
    requires requires(T1 & a,T2 & b){ *a != *b; }
    T1 plain_pattern(
        T1 data_begin,
        T1 data_end,
        size_t data_size,
        T2 pattern_begin,
        T2 pattern_end,
        size_t pattern_size
    ){
        if(pattern_size > data_size){
            return data_end;
        }

        T1 current = data_begin;
        for(size_t i = 0;i < data_size - pattern_size + 1;){
            if constexpr(safe_mode){
                if(current == data_end)break;
            }
            
            bool find = true;
            T2 pattern_current = pattern_begin;
            T1 match_current = current;
            
            for(size_t j = 0;j < pattern_size;){
                if constexpr(safe_mode){
                    if(pattern_current == pattern_end)break;
                }
                
                if(*match_current != *pattern_current){
                    find = false;
                    break;
                }

                ++j;

                ++match_current;
                ++pattern_current;
            }
            if(find)return current;
            
            ++i;
            ++current;
        }
        return data_end;
    }

    template<IsIterator T1,IsIterator T2>
    requires requires(T1 & a,T2 & b){ *a != *b; }
    T1 plain_pattern(
        T1 data_begin,
        T1 data_end,
        T2 pattern_begin,
        T2 pattern_end
    ){
        return plain_pattern<T1,T2,false>(data_begin,data_end,std::distance(data_begin,data_end),pattern_begin,pattern_end,std::distance(pattern_begin,pattern_end));
    }

    template<IsIterator T>
    struct KmpContext{
        std::vector<size_t> next;
        T pattern_begin;
        T pattern_end;

        KmpContext(T begin,T end){
            pattern_begin = begin;
            pattern_end = end;

            calculate();
        }

        void calculate(){
            next.resize(std::distance(pattern_begin,pattern_end),0);
            size_t pattern_size = next.size();

            size_t j = 0;
            for(size_t i = 1;i < pattern_size;++i){
                while(j > 0 && *(pattern_begin + i) != *(pattern_begin + j)){
                    j = next[j - 1];
                }

                if(*(pattern_begin + i) == *(pattern_begin + j)){
                    ++j;
                }

                next[i] = j;
            }
        }
    };

    template<IsIterator T1,IsIterator T2 >
    requires requires(T1 & a,T2 & b){ *a != *b; }
    T1 kmp(
        T1 data_begin,
        T1 data_end,
        T2 pattern_begin,
        T2 pattern_end
    ){
        return kmp<T1,T2>(data_begin,data_end,
            KmpContext<T2>(pattern_begin,pattern_end)
        );
    }

    /// KMP处理
    template<IsIterator T1,IsIterator T2 >
    requires requires(T1 & a,T2 & b){ *a != *b; }
    T1 kmp(
        T1 data_begin,
        T1 data_end,
        const KmpContext<T2> & context
    ){
        T2 pattern_begin = context.pattern_begin;
        T2 pattern_end = context.pattern_end;
        size_t pattern_size = context.next.size();
        auto & next = context.next;

        size_t data_size = std::distance(data_begin,data_end);
        if(pattern_size > data_size){
            return data_end;
        }

        {
            size_t j = 0;
            for(size_t i = 1;i < data_size;){
                while(*(data_begin + i) == *(pattern_begin + j)){
                    ++j;
                    ++i;
                    if(j >= pattern_size) return (data_begin + (i - pattern_size));
                    if(i >= data_size) return data_end;
                }
                while(j > 0 && *(data_begin + i) != *(pattern_begin  + j)){
                    j = next[j - 1];
                }
                if(!j) while(i < data_size && *(data_begin + i) != *pattern_begin) ++i;
            }
        }

        return data_end;
    }

};

#endif