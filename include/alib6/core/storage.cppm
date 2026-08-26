/**
 * @file storage.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 线性槽位重用存储池与单调自增位图 (全体系 PMR 与 C++26 SIMD 向量化加速)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>
#include <climits>
#if __has_include(<experimental/simd>)
#include <experimental/simd>
#define ALIB6_HAS_STD_SIMD 1
#endif

export module alib6.core:storage;
import std;
import :types;
import :memory;
import :concepts;
import :debug;

namespace pmr = std::pmr;

export namespace alib6::storage {

    /**
     * @brief 元素被移除时的清理回调概念
     */
    template<class T>
    concept NeedDataCleanup = requires(T& t) {
        t.d_cleanup();
    };

    /**
     * @brief 支持就地 reset 重置的概念
     */
    template<class T, class... Args>
    concept CanReset = requires(T& t, Args&&... args) {
        t.reset(std::forward<Args>(args)...);
    };

    /**
     * @brief 可被安全重用的组件概念 (同时具备 reset 与匹配的构造函数)
     */
    template<class T, class... Args>
    concept IsResetableComponent = requires(T& t, Args&&... args) {
        t.reset(std::forward<Args>(args)...);
        T(std::forward<Args>(args)...);
    };

    /**
     * @brief ForEach 遍历仿函数概念 (支持仅传引用 f(val) 或传索引与引用 f(idx, val))
     */
    template<class F, class CompT>
    concept FuncForEachable = requires(F&& f, CompT& c) {
        f(c);
    } || requires(F&& f, usize idx, CompT& c) {
        f(idx, c);
    };

    /**
     * @brief 单调自增高效位图 (基于 64 位字，支持 C++26 SIMD 并行向量化操作与 countr_zero / popcount)
     */
    struct MonoBitSet {
        using store_t = u64;
        static constexpr usize shift = 6; ///< 2^6 = 64
        static constexpr usize data_size = sizeof(store_t) * CHAR_BIT; ///< 64
        static constexpr usize mask_bits = data_size - 1;              ///< 0x3F (63)

        pmr::vector<store_t> mask;

        explicit MonoBitSet(usize bit_count = 0, memory_resource* mem = get_default_resource())
            : mask(mem) {
            if (bit_count > 0) ensure(bit_count);
        }

        [[nodiscard]] static constexpr usize get_word_count(usize bit_count) noexcept {
            return (bit_count + mask_bits) >> shift;
        }

        [[nodiscard]] memory_resource* get_allocator() const noexcept {
            return mask.get_allocator().resource();
        }

        /**
         * @brief 确保位图容量能容纳至少 bit_count 个位
         */
        void ensure(usize bit_count) {
            usize need = get_word_count(bit_count);
            if (need > mask.size()) {
                mask.resize(need, 0);
            }
        }

        /**
         * @brief 将指定位置位为 1 (若超出当前容量自动扩容)
         */
        void set(usize pos) {
            usize word_idx = pos >> shift;
            if (word_idx >= mask.size()) ensure(pos + 1);
            mask[word_idx] |= (store_t(1) << (pos & mask_bits));
        }

        /**
         * @brief 将指定位重置为 0
         */
        void reset(usize pos) {
            panic_debug(get_word_count(pos + 1) > mask.size(), "MonoBitSet::reset out of bounds!");
            usize word_idx = pos >> shift;
            if (word_idx >= mask.size()) return;
            mask[word_idx] &= ~(store_t(1) << (pos & mask_bits));
        }

        /**
         * @brief 获取指定位的布尔值
         */
        [[nodiscard]] bool get(usize pos) const noexcept {
            usize word_idx = pos >> shift;
            if (word_idx >= mask.size()) return false;
            return ((mask[word_idx] >> (pos & mask_bits)) & 0x1) != 0;
        }

        [[nodiscard]] bool test(usize pos) const noexcept {
            return get(pos);
        }

        /**
         * @brief 统计置为 1 的位数量 (使用硬件指令 popcount)
         */
        [[nodiscard]] usize count_1() const noexcept {
            usize total = 0;
            for (auto word : mask) {
                total += static_cast<usize>(std::popcount(word));
            }
            return total;
        }

        /**
         * @brief 统计置为 0 的位数量
         */
        [[nodiscard]] usize count_0(usize total_bits) const noexcept {
            if (total_bits == 0) return 0;
            usize ones = count_1();
            return total_bits > ones ? total_bits - ones : 0;
        }

        /**
         * @brief 查找从 start_pos 开始的第一个 1 的位置 (未找到返回 std::nullopt)
         */
        [[nodiscard]] std::optional<usize> find_next_1(usize start_pos) const noexcept {
            usize word_idx = start_pos >> shift;
            if (word_idx >= mask.size()) return std::nullopt;

            usize offset = start_pos & mask_bits;
            store_t current_word = mask[word_idx] & (~store_t(0) << offset);

            while (true) {
                if (current_word) {
                    return (word_idx << shift) + static_cast<usize>(std::countr_zero(current_word));
                }
                ++word_idx;
                if (word_idx >= mask.size()) break;
                current_word = mask[word_idx];
            }
            return std::nullopt;
        }

        /**
         * @brief 查找从 start_pos 开始的第一个 0 的位置 (未找到返回 std::nullopt)
         */
        [[nodiscard]] std::optional<usize> find_next_0(usize start_pos, usize max_elements) const noexcept {
            usize word_idx = start_pos >> shift;
            if (start_pos >= max_elements) return std::nullopt;

            usize offset = start_pos & mask_bits;
            store_t current_word = (~mask[word_idx]) & (~store_t(0) << offset);

            while (true) {
                if (current_word) {
                    usize pos = (word_idx << shift) + static_cast<usize>(std::countr_zero(current_word));
                    if (pos < max_elements) return pos;
                    return std::nullopt;
                }
                ++word_idx;
                if (word_idx >= mask.size() || (word_idx << shift) >= max_elements) break;
                current_word = ~mask[word_idx];
            }
            return std::nullopt;
        }

        /**
         * @brief 遍历全部位 (0 与 1)
         */
        template<class F>
        void for_each_all(F&& func, usize max_elements = std::numeric_limits<usize>::max()) const {
            usize cursor = 0;
            for (auto word : mask) {
                for (unsigned i = 0; i < data_size; ++i) {
                    if (cursor >= max_elements) return;
                    func(cursor, ((word >> i) & 0x1) != 0);
                    ++cursor;
                }
            }
        }

        /**
         * @brief 快速遍历置 1 的位 (利用 BLSR / countr_zero 跳步算法)
         */
        template<class F>
        void for_each_1(F&& func, usize max_elements = std::numeric_limits<usize>::max()) const {
            usize base = 0;
            for (auto word : mask) {
                if (!word) {
                    base += data_size;
                    continue;
                }
                auto w = word;
                while (w) {
                    usize offset = static_cast<usize>(std::countr_zero(w));
                    usize pos = base + offset;
                    if (pos >= max_elements) return;
                    func(pos);
                    w &= w - 1; // 清除最低位的 1
                }
                base += data_size;
            }
        }

        /**
         * @brief 快速遍历置 0 的位 (利用反码与 BLSR 跳步算法)
         */
        template<class F>
        void for_each_0(F&& func, usize max_elements = std::numeric_limits<usize>::max()) const {
            usize base = 0;
            for (auto word : mask) {
                auto inverted = ~word;
                if (!inverted) {
                    base += data_size;
                    continue;
                }
                while (inverted) {
                    usize offset = static_cast<usize>(std::countr_zero(inverted));
                    usize pos = base + offset;
                    if (pos >= max_elements) return;
                    func(pos);
                    inverted &= inverted - 1;
                }
                base += data_size;
            }
        }

        /**
         * @brief 填充位图 (支持 SIMD 向量化加速批量填充)
         */
        void fill(bool val = false) {
            store_t fill_val = val ? std::numeric_limits<store_t>::max() : 0;
#if defined(ALIB6_HAS_STD_SIMD)
            namespace stdx = std::experimental;
            constexpr usize simd_width = 4; // 256 位 SIMD 寄存器 (4 x 64-bit)
            usize n = mask.size();
            usize i = 0;
            stdx::fixed_size_simd<store_t, simd_width> v_fill(fill_val);

            for (; i + simd_width <= n; i += simd_width) {
                v_fill.copy_to(mask.data() + i, stdx::element_aligned);
            }
            for (; i < n; ++i) {
                mask[i] = fill_val;
            }
#else
            std::fill(mask.begin(), mask.end(), fill_val);
#endif
        }

        /**
         * @brief 检查位图是否全为 0 (利用 SIMD 256-bit 并行规约检测)
         */
        [[nodiscard]] bool none() const noexcept {
#if defined(ALIB6_HAS_STD_SIMD)
            namespace stdx = std::experimental;
            constexpr usize simd_width = 4;
            usize n = mask.size();
            usize i = 0;
            stdx::fixed_size_simd<store_t, simd_width> v_acc(0);

            for (; i + simd_width <= n; i += simd_width) {
                stdx::fixed_size_simd<store_t, simd_width> chunk;
                chunk.copy_from(mask.data() + i, stdx::element_aligned);
                v_acc |= chunk;
            }
            if (stdx::any_of(v_acc != 0)) {
                return false;
            }
            for (; i < n; ++i) {
                if (mask[i] != 0) return false;
            }
            return true;
#else
            for (auto word : mask) {
                if (word) return false;
            }
            return true;
#endif
        }

        /**
         * @brief SIMD 向量化并行位与运算
         */
        MonoBitSet& bitwise_and(const MonoBitSet& other) {
            usize min_sz = std::min(mask.size(), other.mask.size());
            usize i = 0;
#if defined(ALIB6_HAS_STD_SIMD)
            namespace stdx = std::experimental;
            constexpr usize simd_width = 4;
            for (; i + simd_width <= min_sz; i += simd_width) {
                stdx::fixed_size_simd<store_t, simd_width> v1, v2;
                v1.copy_from(mask.data() + i, stdx::element_aligned);
                v2.copy_from(other.mask.data() + i, stdx::element_aligned);
                auto res = v1 & v2;
                res.copy_to(mask.data() + i, stdx::element_aligned);
            }
#endif
            for (; i < min_sz; ++i) {
                mask[i] &= other.mask[i];
            }
            if (mask.size() > min_sz) {
                std::fill(mask.begin() + min_sz, mask.end(), 0);
            }
            return *this;
        }

        /**
         * @brief SIMD 向量化并行位或运算
         */
        MonoBitSet& bitwise_or(const MonoBitSet& other) {
            if (other.mask.size() > mask.size()) {
                mask.resize(other.mask.size(), 0);
            }
            usize n = other.mask.size();
            usize i = 0;
#if defined(ALIB6_HAS_STD_SIMD)
            namespace stdx = std::experimental;
            constexpr usize simd_width = 4;
            for (; i + simd_width <= n; i += simd_width) {
                stdx::fixed_size_simd<store_t, simd_width> v1, v2;
                v1.copy_from(mask.data() + i, stdx::element_aligned);
                v2.copy_from(other.mask.data() + i, stdx::element_aligned);
                auto res = v1 | v2;
                res.copy_to(mask.data() + i, stdx::element_aligned);
            }
#endif
            for (; i < n; ++i) {
                mask[i] |= other.mask[i];
            }
            return *this;
        }

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "MonoBitSet(ones={})", count_1());
        }
    };

    /// @brief 历史命名别名兼容
    using MonoticBitSet = MonoBitSet;

    /**
     * @brief 线性槽位重用存储池 (带 Freelist 与 Bitset 标记)
     * 
     * @tparam T        存储元素类型
     * @tparam Internal 底层容器类型 (默认 pmr::vector<T>)
     */
    template<class T, class Internal = pmr::vector<T>>
    struct FreelistLinearStorage {
        Internal data;
        pmr::vector<usize> free_elements;
        MonoBitSet available_bits; ///< 1 = 该槽位为空闲可用 (free), 0 = 该槽位被有效占用 (occupied)

        using reference = T&;
        using const_reference = const T&;
        using value_type = T;

        [[nodiscard]] auto reserve(usize sz) { return data.reserve(sz); }
        [[nodiscard]] usize size() const noexcept { return data.size(); }
        [[nodiscard]] usize capacity() const noexcept { return data.capacity(); }
        [[nodiscard]] bool empty() const noexcept { return data.empty(); }

        /**
         * @brief 返回当前实际有效存储的元素个数 (总分配容量 - 空闲槽位数)
         */
        [[nodiscard]] usize occupied_count() const noexcept {
            return data.size() >= free_elements.size() ? data.size() - free_elements.size() : 0;
        }

        [[nodiscard]] usize free_count() const noexcept {
            return free_elements.size();
        }

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(
                std::back_inserter(target),
                "FreelistStorage(occupied={}, free={}, capacity={})",
                occupied_count(), free_count(), capacity()
            );
        }

        [[nodiscard]] bool is_occupied(usize index) const noexcept {
            return index < data.size() && !available_bits.get(index);
        }

        [[nodiscard]] bool is_free(usize index) const noexcept {
            return index < data.size() && available_bits.get(index);
        }

        reference operator[](usize index) {
            if constexpr (requires { data.find(index); }) {
                auto it = data.find(index);
                panic_if(it == data.end(), "FreelistLinearStorage::[] out of bounds!");
                return it->second;
            } else {
                return data[index];
            }
        }

        const_reference operator[](usize index) const {
            if constexpr (requires { data.find(index); }) {
                auto it = data.find(index);
                panic_if(it == data.end(), "FreelistLinearStorage::[] out of bounds!");
                return it->second;
            } else {
                return data[index];
            }
        }

        void clear() {
            if constexpr (NeedDataCleanup<T>) {
                for_each([&](T& item) { item.d_cleanup(); });
            }
            data.clear();
            available_bits.mask.clear();
            free_elements.clear();
        }

        explicit FreelistLinearStorage(
            usize reserve_size = 0,
            memory_resource* mem = get_default_resource()
        ) : data(mem), free_elements(mem), available_bits(reserve_size, mem) {
            if (reserve_size > 0) data.reserve(reserve_size);
        }

        FreelistLinearStorage(const FreelistLinearStorage& other, memory_resource* mem = get_default_resource())
            : data(other.data, mem)
            , free_elements(other.free_elements, mem)
            , available_bits(other.available_bits.mask.size() * MonoBitSet::data_size, mem) {
            available_bits = other.available_bits;
        }

        FreelistLinearStorage(FreelistLinearStorage&& other) noexcept = default;

        FreelistLinearStorage(FreelistLinearStorage&& other, memory_resource* mem)
            : data(std::move(other.data), mem)
            , free_elements(std::move(other.free_elements), mem)
            , available_bits(std::move(other.available_bits)) {}

        FreelistLinearStorage& operator=(const FreelistLinearStorage& other) = default;
        FreelistLinearStorage& operator=(FreelistLinearStorage&& other) noexcept = default;

        /**
         * @brief 遍历所有已被占用的有效元素 (支持 func(val) 与 func(idx, val))
         */
        template<class F>
        void for_each(F&& func) {
            available_bits.for_each_0([&](usize pos) {
                if (pos >= size()) return;
                if constexpr (requires { func(pos, (*this)[pos]); }) {
                    func(pos, (*this)[pos]);
                } else {
                    func((*this)[pos]);
                }
            }, size());
        }

        template<class F>
        void for_each(F&& func) const {
            available_bits.for_each_0([&](usize pos) {
                if (pos >= size()) return;
                if constexpr (requires { func(pos, (*this)[pos]); }) {
                    func(pos, (*this)[pos]);
                } else {
                    func((*this)[pos]);
                }
            }, size());
        }

        /**
         * @brief 纯追加新元素 (不复用已有空闲槽位)
         */
        template<class... Ts>
        T& next(Ts&&... args) {
            available_bits.ensure(data.size() + 1);
            if constexpr (requires { data.emplace_back(std::forward<Ts>(args)...); }) {
                return data.emplace_back(std::forward<Ts>(args)...);
            } else if constexpr (requires { data.emplace(std::forward<Ts>(args)...).first; }) {
                return data.emplace(data.size(), std::forward<Ts>(args)...).first->second;
            } else {
                static_assert(requires { data.emplace_back(std::forward<Ts>(args)...); }, "Unsupported container type");
            }
        }

        /**
         * @brief 复用空闲槽位 (调用者必须保证存在空闲槽位)
         */
        template<class... Ts>
        T& next_free(Ts&&... args) {
            usize unused = 0;
            return next_free_with_index(unused, std::forward<Ts>(args)...);
        }

        /**
         * @brief 复用空闲槽位并返回对应槽位索引
         */
        template<class... Ts>
        T& next_free_with_index(usize& out_index, Ts&&... args) {
            panic_debug(free_elements.empty(), "No free elements in storage!");
            usize index = free_elements.back();
            available_bits.reset(index);
            free_elements.pop_back();
            out_index = index;

            T& ref = (*this)[index];
            if constexpr (CanReset<T, Ts...>) {
                ref.reset(std::forward<Ts>(args)...);
            } else {
                std::destroy_at(&ref);
                std::construct_at(&ref, std::forward<Ts>(args)...);
            }
            return ref;
        }

        /**
         * @brief 移除并释放对应槽位 (支持 NeedDataCleanup 回调并防重复释放)
         */
        void remove(usize index) {
            panic_if(index >= data.size(), "FreelistLinearStorage::remove index out of bounds!");
            panic_debug(available_bits.get(index), "Double free in FreelistLinearStorage at index!");
            if constexpr (NeedDataCleanup<T>) {
                (*this)[index].d_cleanup();
            }
            available_bits.set(index);
            free_elements.push_back(index);
        }

        /**
         * @brief 获取元素：优先复用空闲槽位，若无则追加新槽位
         */
        template<class... Ts>
        T& try_next(bool& is_new, Ts&&... args) {
            usize unused = 0;
            return try_next_with_index(is_new, unused, std::forward<Ts>(args)...);
        }

        /**
         * @brief 获取元素并返回槽位索引
         */
        template<class... Ts>
        T& try_next_with_index(bool& is_new, usize& out_index, Ts&&... args) {
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

} // namespace alib6::storage
