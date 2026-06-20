/**
 * @file astorage.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些奇奇怪怪的数据结构 / Miscellaneous data structures
 * @version 5.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef ALIB5_ASTORAGE_H
#define ALIB5_ASTORAGE_H

#include <alib5/autil.h>
#include <alib5/adebug.h>
#include <limits>
#include <vector>
#include <bit>
#include <memory_resource>

namespace alib5::storage {

    //// Concepts ////

    /**
     * @brief Concept requiring a d_cleanup() member invoked right after the storage removes an element.
     */
    template<class T> concept NeedDataCleanup =
        requires(T& t){ t.d_cleanup(); };

    /**
     * @brief Concept requiring a reset(Args...) member usable to reinitialize a stored value.
     */
    template<class T, class... Args> concept CanReset = requires(T & t, Args&&... args) {
        t.reset(std::forward<Args>(args)...);
    };

    /**
     * @brief Concept requiring both reset(Args...) and a matching constructor so reuse can be safe.
     */
    template<class T, class... Args> concept IsResetableComponent = requires(T & t, Args&&... args) {
        t.reset(std::forward<Args>(args)...);
        T(std::forward<Args>(args)...);
    };

    /**
     * @brief Concept constraining foreach-style functors callable with a value reference.
     */
    template<class T, class CompT> concept FuncForEachable = requires(T && t, CompT & c) { t(c); };

    //// MonoBitSet ////

    /**
     * @brief Monotonically growing bitset backed by a vector of 64-bit words.
     *
     * Supports fast iteration over set/zero bits via countr_zero, and bounds-checked
     * single-bit operations.
     */
    struct ALIB5_API MonoBitSet {
        using store_t = uint64_t;
        static constexpr size_t data_size = sizeof(store_t) * __CHAR_BIT__;

        std::vector<store_t> mask;

        /// @brief Compute how many store_t words are needed for the given element index.
        static constexpr size_t get_ecount(size_t count) {
            return count / data_size + 1;
        }

        /// @brief Ensure the bitset has enough words to address the given element count.
        inline void ensure(size_t count) {
            size_t need = get_ecount(count);
            if (need > mask.size())
                mask.resize(need, 0);
        }

        /// @brief Set the bit at @p pos.
        inline void set(size_t pos) {
            panic_debug(get_ecount(pos) > mask.size(), "MonoBitSet::set out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;
            mask[base] |= (store_t(1) << offset);
        }

        /// @brief Reset (clear) the bit at @p pos.
        inline void reset(size_t pos) {
            panic_debug(get_ecount(pos) > mask.size(), "MonoBitSet::reset out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;
            mask[base] &= ~(store_t(1) << offset);
        }

        /// @brief Read the bit at @p pos.
        inline bool get(size_t pos) const {
            panic_debug(get_ecount(pos) > mask.size(), "MonoBitSet::get out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;
            return (mask[base] >> offset) & 0x1;
        }

        /// @brief Walk every bit (0 and 1) up to @p max_elements, calling func(index, bit_value).
        inline void for_each_all(auto&& func, size_t max_elements = SIZE_MAX) const {
            size_t cursor = 0;
            for (auto word : mask) {
                for (unsigned i = 0; i < data_size; ++i) {
                    if (cursor >= max_elements) return;
                    func(cursor, (word >> i) & 0x1);
                    ++cursor;
                }
            }
        }

        /// @brief Walk only set bits via countr_zero, calling func(position).
        inline void for_each_1(auto&& func, size_t max_elements = SIZE_MAX) const {
            size_t base = 0;
            for (auto word : mask) {
                if (!word) { base += data_size; continue; }
                auto w = word;
                while (w) {
                    size_t offset = std::countr_zero(w);
                    size_t pos = base + offset;
                    if (pos >= max_elements) return;
                    func(pos);
                    w &= w - 1;  // clear lowest set bit
                }
                base += data_size;
            }
        }

        /// @brief Walk only zero bits (i.e. ~mask) via countr_zero, calling func(position).
        inline void for_each_0(auto&& func, size_t max_elements = SIZE_MAX) const {
            size_t base = 0;
            for (auto word : mask) {
                auto inverted = ~word;
                if (!inverted) { base += data_size; continue; }
                while (inverted) {
                    size_t offset = std::countr_zero(inverted);
                    size_t pos = base + offset;
                    if (pos >= max_elements) return;
                    func(pos);
                    inverted &= inverted - 1;
                }
                base += data_size;
            }
        }

        /// @brief Fill every word with all-ones (@p val=true) or all-zeros (@p val=false).
        inline void fill(bool val = false) {
            store_t fill_val = val ? std::numeric_limits<store_t>::max() : 0;
            std::fill(mask.begin(), mask.end(), fill_val);
        }

        /// @brief Check if the bitset has no set bits up to the populated range.
        inline bool none() const {
            for (auto word : mask)
                if (word) return false;
            return true;
        }

        inline MonoBitSet(size_t elements = 0) { ensure(elements); }
    };

    //// FreelistLinearStorage ////

    /**
     * @brief Linear storage combining a vector, a free list, and a monotonic bitset for slot availability.
     *
     * @tparam T        Stored value type.
     * @tparam Internal Backing container type (default: pmr::vector<T>).
     *
     * When reusing a freed slot, reset(Args...) is preferred over destruct+construct if available.
     */
    template<class T, class Internal = std::pmr::vector<T>>
    struct ALIB5_API FreelistLinearStorage {
        Internal data;
        std::pmr::vector<size_t> free_elements;
        MonoBitSet available_bits;  ///< 1 = slot is free

        using reference = T&;
        using const_reference = const T&;
        using value_type = T;

        //// Capacity & access ////

        inline auto reserve(size_t sz) { return data.reserve(sz); }
        inline size_t size() const { return data.size(); }
        inline size_t capacity() const { return data.capacity(); }
        inline bool empty() const { return data.empty(); }

        /// @brief Check whether the slot at @p index is occupied (not free).
        inline bool is_occupied(size_t index) const {
            return index < available_bits.mask.size() * MonoBitSet::data_size
                && !available_bits.get(index);
        }

        /// @brief Check whether the slot at @p index is free.
        inline bool is_free(size_t index) const {
            return index < available_bits.mask.size() * MonoBitSet::data_size
                && available_bits.get(index);
        }

        inline reference operator[](size_t index) {
            if constexpr (requires { data.find(index); }) {
                auto it = data.find(index);
                panic_if(it == data.end(), "FreelistLinearStorage::[] out of bounds!");
                return it->second;
            } else {
                return data[index];
            }
        }

        inline const_reference operator[](size_t index) const {
            if constexpr (requires { data.find(index); }) {
                auto it = data.find(index);
                panic_if(it == data.end(), "FreelistLinearStorage::[] out of bounds!");
                return it->second;
            } else {
                return data[index];
            }
        }

        //// Lifecycle ////

        inline void clear() {
            data.clear();
            available_bits.mask.clear();
            free_elements.clear();
        }

        inline FreelistLinearStorage(
            size_t reserve_size = 0,
            std::pmr::memory_resource* mr_data = std::pmr::get_default_resource(),
            std::pmr::memory_resource* mr_free = std::pmr::get_default_resource()
        ) : data(mr_data), free_elements(mr_free) {
            data.reserve(reserve_size);
            available_bits.ensure(reserve_size);
        }

        inline FreelistLinearStorage(const FreelistLinearStorage& other, std::pmr::memory_resource* mr)
            : data(other.data, mr), free_elements(other.free_elements, mr), available_bits(other.available_bits) {}

        inline FreelistLinearStorage(FreelistLinearStorage&& other) ALIB5_NOEXCEPT
            : data(std::move(other.data))
            , free_elements(std::move(other.free_elements))
            , available_bits(std::move(other.available_bits)) {}

        inline FreelistLinearStorage(FreelistLinearStorage&& other, std::pmr::memory_resource* mr)
            : data(std::move(other.data), mr)
            , free_elements(std::move(other.free_elements), mr)
            , available_bits(std::move(other.available_bits)) {}

        FreelistLinearStorage& operator=(const FreelistLinearStorage& other) {
            if (this == &other) [[unlikely]] return *this;
            data = other.data;
            free_elements = other.free_elements;
            available_bits = other.available_bits;
            return *this;
        }

        FreelistLinearStorage& operator=(FreelistLinearStorage&& other) ALIB5_NOEXCEPT {
            if (this == &other) [[unlikely]] return *this;
            data = std::move(other.data);
            free_elements = std::move(other.free_elements);
            available_bits = std::move(other.available_bits);
            return *this;
        }

        //// Iteration ////

        /**
         * @brief Call func on every occupied slot.
         *
         * Uses MonoBitSet::for_each_0 because available_bits marks free slots as 1,
         * so occupied slots are the zero bits.
         */
        template<FuncForEachable<T> F>
        void for_each(F&& func) {
            available_bits.for_each_0([&](size_t pos) {
                if (pos >= size()) return;
                func((*this)[pos]);
            }, size());
        }

        //// Element insertion ////

        /// @brief Append a new element (no slot reuse). Returns a reference to it.
        template<class... Ts>
        inline T& next(Ts&&... args) {
            available_bits.ensure(data.size() + 1);
            if constexpr (requires { data.emplace_back(std::forward<Ts>(args)...); }) {
                return data.emplace_back(std::forward<Ts>(args)...);
            } else if constexpr (requires { data.emplace(std::forward<Ts>(args)...).first; }) {
                return data.emplace(data.size(), std::forward<Ts>(args)...).first->second;
            } else {
                static_assert(requires { data.emplace_back(std::forward<Ts>(args)...); }, "Unsupported container type");
            }
        }

        /// @brief Reuse the next free slot. Caller must guarantee a free slot exists.
        template<class... Ts>
        inline T& next_free(Ts&&... args) {
            size_t unused = 0;
            return next_free_with_index(unused, std::forward<Ts>(args)...);
        }

        /// @brief Reuse the next free slot, writing the reused index to @p out_index.
        template<class... Ts>
        inline T& next_free_with_index(size_t& out_index, Ts&&... args) {
            panic_debug(free_elements.empty(), "No free elements in storage!");
            size_t index = free_elements.back();
            available_bits.reset(index);
            free_elements.pop_back();
            out_index = index;

            T& ref = (*this)[index];
            if constexpr (CanReset<T, Ts...>) {
                ref.reset(std::forward<Ts>(args)...);
            } else {
                ref.~T();
                new (&ref) T(std::forward<Ts>(args)...);
            }
            return ref;
        }

        //// Element removal ////

        /// @brief Mark the slot at @p index as free, invoking d_cleanup() if present.
        inline void remove(size_t index) {
            if (available_bits.get(index)) {
                panic_debug(true, "Double free in storage at index!");
                return;
            }
            if constexpr (NeedDataCleanup<T>) {
                (*this)[index].d_cleanup();
            }
            available_bits.set(index);
            free_elements.push_back(index);
        }

        //// Combined try_next ////

        /// @brief Obtain an element: reuse a free slot if any, otherwise append.
        template<class... Ts>
        inline T& try_next(bool& is_new, Ts&&... args) {
            size_t unused = 0;
            return try_next_with_index(is_new, unused, std::forward<Ts>(args)...);
        }

        /// @brief Obtain an element with the slot index reported.
        template<class... Ts>
        inline T& try_next_with_index(bool& is_new, size_t& out_index, Ts&&... args) {
            if (free_elements.empty()) {
                is_new = true;
                out_index = data.size();
                return next(std::forward<Ts>(args)...);
            } else {
                is_new = false;
                return next_free_with_index(out_index, std::forward<Ts>(args)...);
            }
        }
    };

} // namespace alib5::storage

#endif
