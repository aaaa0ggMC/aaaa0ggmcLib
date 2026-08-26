/**
 * @file algo.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 算法子系统聚合导出模块 (alib6.algo)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.algo;

export import :base;
export import :sort;
export import :search;

export namespace alib6 {
    namespace algo = alib6::algo;
}
