/**
 * @file linear_storage.h
 * @brief Compatibility shim — prefer <alib5/astorage.h> directly.
 * @author aaaa0ggmc
 * @date 2026/06/20
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef AECS_LINEAR_STORAGE_H_INCLUDED
#define AECS_LINEAR_STORAGE_H_INCLUDED

#include <alib5/astorage.h>

namespace alib5::ecs::detail {
    using MonoticBitSet = alib5::storage::MonoBitSet;
    template<class T, class Internal = std::pmr::vector<T>>
    using LinearStorage = alib5::storage::FreelistLinearStorage<T, Internal>;
} // namespace alib5::ecs::detail

#endif
