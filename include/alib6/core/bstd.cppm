/**
 * @file bstd.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 为std增加一部分功能，比如透明哈希 (better std)
 * @version 5.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:bstd;
import std;
import :types;

export namespace alib6{
    /**
    * @brief Transparent string hashing.
    * @details Avoids the need to build temporary pmr strings every time.
    * @par Original Comment:
    * 透明哈希处理,不然每次都需要构建临时pmr
    */
    struct TransparentStringHash {
        using is_transparent = void;

        usize operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };

    /**
    * @brief Transparent string equality checker.
    */
    struct TransparentStringEqual {
        using is_transparent = void;

        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
            return lhs == rhs;
        }
    };
}