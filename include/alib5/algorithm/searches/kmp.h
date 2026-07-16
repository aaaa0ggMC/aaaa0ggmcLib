/**
 * @file kmp.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief KMP算法,因为跳转太多,只支持RandomAccessIterator
 * @version 5.0
 * @date 2026-07-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SEARCH_KMP
#define ALIB5_AALGO_SEARCH_KMP
#include <alib5/algorithm/searches/base.h>
#include <vector>

namespace alib5::algo::search{
    
    /**
     * @brief KMP string searching pre-calculated context.
     * 
     * @tparam T Iterator type for the pattern sequence.
     */
    template<
        IsRandomAccessIterator T
    >
    struct KmpContext {
        std::vector<size_t> next;
        T pattern_begin;
        T pattern_end;

        KmpContext(T begin, T end);
        void calculate();
    };


    /**
     * @brief KMP pattern matching processing executing standard matching flow.
     * 
     * @par Notes / 备注:
     * Knuth-Morris-Pratt algorithmic processing logic. / KMP处理
     */
    template<
        IsRandomAccessIterator T1,
        IsRandomAccessIterator T2
    >
    requires IsComparableIterators<T1, T2>
    T1 kmp(T1 data_begin, T1 data_end, const KmpContext<T2>& context){
        T2 pattern_begin = context.pattern_begin;
        T2 pattern_end = context.pattern_end;
        size_t pattern_size = context.next.size();
        auto& next = context.next;

        size_t data_size = std::distance(data_begin, data_end);
        if (pattern_size > data_size) {
            return data_end;
        }

        {
            size_t j = 0;
            for (size_t i = 1; i < data_size;) {
                while (*(data_begin + i) == *(pattern_begin + j)) {
                    ++j;
                    ++i;
                    if (j >= pattern_size) return (data_begin + (i - pattern_size));
                    if (i >= data_size) return data_end;
                }
                while (j > 0 && *(data_begin + i) != *(pattern_begin + j)) {
                    j = next[j - 1];
                }
                if (!j) {
                    while (i < data_size && *(data_begin + i) != *pattern_begin) ++i;
                }
            }
        }
        return data_end;
    }

    /**
     * @brief Simple API for KMP matching when Context is not explicitly provided (auto-generates context).
     */
    template<
        IsRandomAccessIterator T1,
        IsRandomAccessIterator T2
    >
    requires IsComparableIterators<T1, T2>
    T1 kmp(T1 data_begin, T1 data_end, T2 pattern_begin, T2 pattern_end){
        return kmp<T1, T2>(data_begin, data_end, KmpContext<T2>(pattern_begin, pattern_end));
    }
}

namespace alib5::algo::search{
    template<IsRandomAccessIterator T>
    KmpContext<T>::KmpContext(T begin, T end) {
        pattern_begin = begin;
        pattern_end = end;
        calculate();
    }

    template<IsRandomAccessIterator T>
    void KmpContext<T>::calculate() {
        next.resize(std::distance(pattern_begin, pattern_end), 0);
        size_t pattern_size = next.size();

        size_t j = 0;
        for (size_t i = 1; i < pattern_size; ++i) {
            while (j > 0 && *(pattern_begin + i) != *(pattern_begin + j)) {
                j = next[j - 1];
            }
            if (*(pattern_begin + i) == *(pattern_begin + j)) {
                ++j;
            }
            next[i] = j;
        }
    }
}

#endif