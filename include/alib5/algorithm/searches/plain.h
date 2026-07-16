/**
 * @file plain.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 朴素搜索算法
 * @version 5.0
 * @date 2026-07-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SEARCH_PLAIN
#define ALIB5_AALGO_SEARCH_PLAIN
#include <alib5/algorithm/searches/base.h>

namespace alib5::algo::search{
    /**
     * @brief Naive pattern matching algorithm. Time complexity O(N*M).
     * 
     * @par Notes / 备注:
     * Naive pattern matching. / 朴素的模式匹配
     */
    template<
        IsGenericIterator T1, 
        IsGenericIterator T2,
        bool safe_mode = true
    >
    requires IsComparableIterators<T1, T2>
    T1 plain_pattern(T1 data_begin, T1 data_end, T2 pattern_begin, T2 pattern_end){
        if(pattern_begin == pattern_end){
            return data_begin;
        }

        /// 当长度计算代价是O(1)的时候(iterator之间支持减法)
        if constexpr(
            std::sized_sentinel_for<T1,T1> && 
            std::sized_sentinel_for<T2, T2>
        ){
            auto data_size = std::distance(data_begin,data_end);
            auto pattern_size = std::distance(pattern_begin,pattern_end);

            if(pattern_size > data_size){
                return data_end;
            }
        }

        for(T1 current = data_begin; current != data_end; ++current){
            T1 match_current = current;
            T2 pattern_current = pattern_begin;

            while(true){
                if (pattern_current == pattern_end) {
                    return current;
                }
                if(match_current == data_end){
                    return data_end; 
                }
                if(*match_current != *pattern_current) {
                    break;
                }

                ++match_current;
                ++pattern_current;
            }
        }
        return data_end;
    }
}

#endif