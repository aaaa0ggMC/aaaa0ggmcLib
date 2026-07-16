/**
 * @file search.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些基础的搜索算法 
 * @version 5.0
 * @date 2026-07-16
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SEARCH
#define ALIB5_AALGO_SEARCH

#define ALIB5_SEARCH_BASE(X) <alib5/algorithm/searches/X>

/// 朴素匹配
#include ALIB5_SEARCH_BASE(plain.h)
/// KMP
#include ALIB5_SEARCH_BASE(kmp.h)

#undef ALIB5_SEARCH_BASE

#endif