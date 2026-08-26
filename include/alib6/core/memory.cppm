/**
 * @file memory.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 内存管理
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:memory;
import std;

export namespace alib6{
    using memory_resource = std::pmr::memory_resource;

    inline memory_resource* get_default_resource(){
        return std::pmr::get_default_resource();
    }

}
