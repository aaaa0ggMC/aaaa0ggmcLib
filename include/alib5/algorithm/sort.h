/**
 * @file sort.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 常见排序算法, std::less 为递增序列
 * @version 5.0
 * @date 2026-07-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AALGO_SORT_H_INCLUDED
#define ALIB5_AALGO_SORT_H_INCLUDED

//// RANDOM ACCESS ALGORITHMS  ////
#define ALIB5_ALGO_BASE(X) <alib5/algorithm/sorts/random_access/X>

#ifdef ALIB5_ALGO_ENABLE_ENTERTAIN 
/// 猴子排序变体
#include ALIB5_ALGO_BASE(bozo.h)
/// 睡眠排序
#include ALIB5_ALGO_BASE(sleep.h)
#endif

/// 冒泡排序
#include ALIB5_ALGO_BASE(bubble.h)
/// 鸡尾酒排序
#include ALIB5_ALGO_BASE(cocktail.h)
/// 梳排序
#include ALIB5_ALGO_BASE(comb.h)
/// 地精排序
#include ALIB5_ALGO_BASE(gnome.h)
/// 插入排序
#include ALIB5_ALGO_BASE(insertion.h)
/// 奇偶排序
#include ALIB5_ALGO_BASE(odd_even.h)
/// 快速排序
#include ALIB5_ALGO_BASE(quick.h)
/// 选择排序
#include ALIB5_ALGO_BASE(selection.h)
/// 希尔排序
#include ALIB5_ALGO_BASE(shell.h)

#undef ALIB5_ALGO_BASE

#endif