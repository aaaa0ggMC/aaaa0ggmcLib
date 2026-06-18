/**
 * @file linear_storage.h
 * @brief Linear storage backed by a vector with a free list and monotonic bitset. / 线性存储类，目前使用vector，后期可以变成sparse_set啥的
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef AECS_LINEAR_STORAGE_H_INCLUDED
#define AECS_LINEAR_STORAGE_H_INCLUDED
#include <alib5/autil.h>
#include <alib5/adebug.h>
#include <limits>
#include <vector>
#include <bit>
#include <memory_resource>
#include <alib5/ecs/component_concepts.h>

namespace alib5::ecs::detail{
    /**
     * @brief Concept requiring a reset(Args...) member usable to reinitialize a stored value.
     * @par Original Comment:
     * 是否可以通过reset方法实现建议重构？
     */
    template<class T,class... Args> concept CanReset = requires(T & t,Args&&... args){
        t.reset(std::forward<Args>(args)...);
    };
    /**
     * @brief Concept requiring both reset(Args...) and a matching constructor so reuse can be safe.
     * @par Original Comment:
     * 一个合格的component必须要保证有reset方法，并且reset和构造函数的初始化列表建议一致！
     */
    template<class T,class... Args> concept IsResetableComponent = requires(T & t,Args&&... args){
        t.reset(std::forward<Args>(args)...);
        T(std::forward<Args>(args)...);
    };
    /**
     * @brief Concept constraining foreach-style functors callable with a component reference.
     * @par Original Comment:
     * 可以用来foreach的函数，对参数进行限制
     */
    template<class T,class CompT> concept FuncForEachable = requires(T && t,CompT & c){t(c);};

    /**
     * @brief Monotonically growing bitset backed by a vector of 64-bit words.
     * @par Original Comment:
     * 单调递增的bitset
     */
    struct ALIB5_API MonoticBitSet{
        /// @brief 单条数据类型 / single word type
        using store_t = uint64_t;
        /// @brief 单条数据的大小 / bits per single word
        constexpr static size_t data_size = sizeof(store_t) * __CHAR_BIT__;
        /// @brief 数据集 / underlying word vector
        std::vector<store_t> mask;

        /**
         * @brief Compute how many store_t words are needed to address the given element index.
         * @param count 元素index / element index
         * @return mask的index / index into mask
         * @par Original Comment:
         * 获取对应index对应的第几个store_t变量
         */
        inline size_t get_ecount(size_t count){
            size_t ecount = count / data_size + 1;
            return ecount;
        }

        /**
         * @brief Ensure the bitset has enough words to address the given element count.
         * @param count 元素数量 / number of elements
         * @par Original Comment:
         * 确保数据集里面支持这么多元素
         */
        inline void ensure(size_t count){
            size_t ecount = get_ecount(count);

            if(ecount > mask.size()){
                mask.resize(ecount,0);
            }
        }

        /**
         * @brief Set the bit at the given position.
         * @par Original Comment:
         * 置位对应元素的位置
         */
        inline void set(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            mask[base] |= ((store_t)1 << offset);
        }

        /**
         * @brief Reset (clear) the bit at the given position.
         * @par Original Comment:
         * 重置对应位置的元素
         */
        inline void reset(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            mask[base] &= ~((store_t)1 << offset);
        }

        /**
         * @brief Read the bit at the given position.
         * @return 是否位置 / bit value at the position
         * @par Original Comment:
         * 获取对应位置数据的内容
         */
        inline bool get(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            return (mask[base] >> offset) & 0x1;
        }

        /**
         * @brief Invoke func for every position up to max_elements, passing the index and bit value.
         * @param func 相应函数，第一个为位置，第二个为位的状态 / callback taking index and bit state
         * @param max_elements 最大遍历元素数 / maximum index to iterate
         * @par Original Comment:
         * 遍历
         */
        inline void for_each(auto && func,size_t max_elements = SIZE_MAX){
            size_t all = 0;
            for(auto t : mask){
                for(unsigned int i = 0;i < data_size;++i){
                    if(all >= max_elements)return;
                    func(all,(t >> i) & 0x1);
                    ++all;
                }
            }
        }

        /**
         * @brief Invoke func only for set positions, skipping zero bits entirely.
         * @param func 相应函数，只需要一个参数（元素位置） / callback taking only the position
         * @param max_elements 最大的遍历到的位置 / maximum index to iterate
         * @par Original Comment:
         * 遍历，但是跳过为0的位
         */
        inline void for_each_skip_0_bits(auto && func,size_t max_elements = SIZE_MAX){
            size_t all = 0;
            for(auto t : mask){
                if(!t){
                    all += data_size;
                    continue;
                }
                for(unsigned int i = 0;i < data_size;++i){
                    if(all >= max_elements)return;
                    if((t >> i) & 0x1)func(all);
                    ++all;
                }
            }
        }

        /**
         * @brief Invoke func only for unset positions, skipping fully set words.
         * @param func 相应函数，只需要一个参数（元素位置） / callback taking only the position
         * @param max_elements 最大的遍历到的位置 / maximum index to iterate
         * @par Original Comment:
         * 遍历，但是跳过为1的位
         */
        inline void for_each_skip_1_bits(auto && func,size_t max_elements = SIZE_MAX){
            size_t all = 0;
            for(auto t : mask){
                if(t == std::numeric_limits<store_t>::max()){
                    all += data_size;
                    continue;
                }
                for(unsigned int i = 0;i < data_size;++i){
                    if(all >= max_elements)return;
                    if((~(t >> i) & 0x1))func(all);
                    ++all;
                }
            }
        }

        /**
         * @brief Fill every word with all-ones or all-zeros.
         * @param val 值，默认false / fill value, false by default
         * @par Original Comment:
         * 清除所有位为0/1
         */
        inline void clear(bool val = false){
            store_t fill_num = val?std::numeric_limits<store_t>::max():0;
            std::fill(mask.begin(),mask.end(),fill_num);
        }

        /**
         * @brief Construct the bitset ensuring capacity for the given element count.
         * @param elements 初始化大小 / initial element count
         * @par Original Comment:
         * 构造函数
         */
        inline MonoticBitSet(size_t elements = 0){
            ensure(elements);
        }
    };

    /**
     * @brief Linear storage combining a vector, a free list, and a monotonic bitset for slot availability.
     * @tparam T 存储的内部类型，建议为trival-copyable / stored value type
     * @tparam Internal 内部容器类型 / backing container type
     * @start-date 2025/11/27
     * @note 内部类型支持的注入有reset(...)，会在复用的时候调用，要是不存在reset会选择构造函数
     *  因此建议构造函数和reset签名一致，但是不是硬性要求
     */
    template<class T,class Internal = std::pmr::vector<T>> struct ALIB5_API LinearStorage{
        /// @brief 目前的内部数据类型 / current internal data container
        Internal         data;
        /// @brief 空闲数据块列表 / free slot index list
        std::pmr::vector<size_t>     free_elements;
        /// @brief 用于遍历的列表 / bitset tracking slot availability
        MonoticBitSet          available_bits;

        //// 支持ref直接引用 ////
        /// 引用类型 / reference type aliases
        using reference = T&;
        using const_reference = const T&;
        /// 基础类型 / value type alias
        using value_type = T;
        /// reserve数据 / forward reserve to the underlying container
        inline auto reserve(size_t sz){
            return data.reserve(sz);
        }
        /// 重载[]获取对应的引用 / access element by index, supporting map-like containers
        inline reference operator[](size_t index){
            if constexpr(requires{data.find(index);}) {
                auto it = data.find(index);
                panic_if(it == data.end(), "Out of bounds!");
                return it->second;
            }else{
                return data[index];
            }
        }
        /// 重载[]获取对应的常量引用 / const access element by index, supporting map-like containers
        inline const_reference operator[](size_t index) const {
            if constexpr(requires{data.find(index);}) {
                auto it = data.find(index);
                panic_if(it == data.end(), "Out of bounds!");
                return it->second;
            }else{
                return data[index];
            }
        }
        /// 获取当前的数据量 / return current element count
        inline size_t size() const {return data.size();}
        /// 获取当前是否为空 / return whether the storage is empty
        inline bool empty() const {return data.empty();}

        /// 清除当前容器 / clear all stored data and reset free list / bits
        inline void clear(){
            // 清除数据，这里是假装回到初始状态
            data.clear();
            available_bits.mask.clear();
            free_elements.clear();
        }

        /**
         * @brief Construct a LinearStorage with an optional reservation size and memory resources.
         * @param reserve_size 预留大小 / reservation size
         * @param __data 数据容器使用的内存资源 / memory resource for data
         * @param __free_list 空闲列表使用的内存资源 / memory resource for the free list
         * @par Original Comment:
         * 构造函数
         */
        inline LinearStorage(size_t reserve_size = 0,
            std::pmr::memory_resource * __data = std::pmr::get_default_resource(),
            std::pmr::memory_resource * __free_list = std::pmr::get_default_resource()
        ):data(__data),free_elements(__free_list){
            data.reserve(reserve_size);
            available_bits.ensure(reserve_size);
        }

        /// @brief Copy-construct using the given allocator. / 拷贝构造并指定分配器
        inline LinearStorage(const LinearStorage& other, std::pmr::memory_resource* __a)
        :data(other.data, __a)
        ,free_elements(other.free_elements, __a)
        ,available_bits(other.available_bits)
        {}

        /// @brief Move-construct without allocating. / 移动构造
        inline LinearStorage(LinearStorage&& other) ALIB5_NOEXCEPT
        :data(std::move(other.data))
        ,free_elements(std::move(other.free_elements))
        ,available_bits(std::move(other.available_bits))
        {}

        /// @brief Move-construct with a specified allocator. / 移动构造并指定分配器
        inline LinearStorage(LinearStorage&& other, std::pmr::memory_resource* __a)
        :data(std::move(other.data), __a)
        ,free_elements(std::move(other.free_elements), __a)
        ,available_bits(std::move(other.available_bits))
        {}

        /// @brief Copy-assign from another LinearStorage. / 拷贝赋值
        LinearStorage& operator=(const LinearStorage& other){
            if (this == &other) [[unlikely]] return *this;

            data = other.data;
            free_elements = other.free_elements;
            available_bits = other.available_bits;
            return *this;
        }

        /// @brief Move-assign from another LinearStorage. / 移动赋值
        LinearStorage& operator=(LinearStorage&& other) ALIB5_NOEXCEPT{
            if (this == &other) [[unlikely]] return *this;

            data = std::move(other.data);
            free_elements = std::move(other.free_elements);
            available_bits = std::move(other.available_bits);
            return *this;
        }

        /**
         * @brief Invoke func for each currently-occupied slot.
         * @par Original Comment:
         * 进行遍历
         */
        template<FuncForEachable<T> F> void for_each(F && func){
            size_t stop_at = size();
            size_t processed = 0;

            for(size_t i = 0;processed < stop_at && i < available_bits.mask.size();++i){
                MonoticBitSet::store_t inverted_mask = available_bits.mask[i];
                size_t base_index = i * MonoticBitSet::data_size;
                if(base_index >= stop_at)break;
                if(inverted_mask == std::numeric_limits<MonoticBitSet::store_t>::max()){
                    continue;
                }

                inverted_mask = ~inverted_mask;

                while(inverted_mask){
                    size_t offset = std::countr_zero(inverted_mask);
                    size_t current_index = offset + base_index;

                    if(current_index >= stop_at)break;
                    func(this->operator[](current_index));

                    inverted_mask &= inverted_mask - 1;
                }
            }
        }

        /**
         * @brief Append a new element (no slot reuse) and return a reference to it.
         * @param ...args 传入对应类型构造函数的参数 / constructor arguments
         * @return 对象的引用 / reference to the appended element
         * @par Original Comment:
         * 获取下一个对象
         */
        template<class...Ts> inline T& next(Ts&&... args){
            available_bits.ensure(data.size() + 1);
            if constexpr(requires{data.emplace_back(std::forward<Ts>(args)...);}){
                return data.emplace_back(std::forward<Ts>(args)...);
            }else if constexpr(requires{data.emplace(std::forward<Ts>(args)...).first;}){
                return data.emplace(
                    data.size(), // 递增数列
                    std::forward<Ts>(args)...
                ).first->second;
            }else{
                static_assert(requires{data.emplace_back(std::forward<Ts>(args)...);},"Usupported types");
            }
        }

        /**
         * @brief Reuse the next free slot without bounds checking (caller must ensure a free slot exists).
         * @param ...args reset/构造函数 的参数列表 / reset or constructor arguments
         * @return 对应引用 / reference to the reused element
         * @par Original Comment:
         * 获取空闲列表的下一个对象，release不会检测是否非空因此你需要自己保证非空，建议try_next
         */
        template<class...Ts> inline T& next_free(Ts&&... args){
            size_t index = 0;
            return next_free_with_index(index,std::forward<Ts>(args)...);
        }

        /**
         * @brief Reuse the next free slot, writing the reused index to i_index.
         * @param i_index 引用在线性存储中的序号 / output index of the reused slot
         * @param ...args reset/构造函数 的参数列表 / reset or constructor arguments
         * @return 对应引用 / reference to the reused element
         * @par Original Comment:
         * 获取空闲列表的下一个对象，release不会检测是否非空因此你需要自己保证非空，建议try_next
         */
        template<class...Ts> inline T& next_free_with_index(size_t& i_index,Ts&&... args){
            panic_debug(free_elements.empty(),"There are no free elements in storage!");
            size_t index = free_elements.back();
            available_bits.reset(index);
            free_elements.pop_back();

            i_index = index;

            T & ret = (*this)[index];
            if constexpr(CanReset<T,Ts...>){
                // 这一块暂时按下不表，后面需要的时候再适配看看
                // static_assert(IsResetableComponent<T,Ts...>,"Reset function must have the same argument list with the constructor!");
                ret.reset(std::forward<Ts>(args)...);
            }else{
                ret.~T();
                new (&ret) T(std::forward<Ts>(args)...);
            }
            return ret;
        }

        /**
         * @brief Remove (mark free) the element at the given index, invoking d_cleanup if present.
         * @param index 数据的位置序号 / index to remove
         * @par Original Comment:
         * 移除对应序号的数据
         */
        inline void remove(size_t index){
            if(available_bits.get(index)){
                panic_debug(true,"Double free a storage!");
                return;
            }
            if constexpr(NeedDataCleanup<T>){
                (*this)[index].d_cleanup();
            }
            available_bits.set(index);
            free_elements.push_back(index);
        }

        /**
         * @brief Try to obtain an element: reuse a free slot if any, otherwise append a new one.
         * @param flag true:新对象  false:重用对象 / output flag indicating a new object vs reused
         * @param ...args reset/构造函数 的参数列表 / reset or constructor arguments
         * @return 对象引用 / reference to the obtained element
         * @par Original Comment:
         * 尝试获取下一个对象，存在空闲对象使用next_free否则使用next
         */
        template<class... Ts> inline T& try_next(bool & flag,Ts&&... args){
            size_t index = 0;
            return try_next_with_index(flag,index,std::forward<Ts>(args)...);
        }

        /**
         * @brief Try to obtain an element, also reporting the slot index used.
         * @param flag true:新对象  false:重用对象 / output flag indicating a new object vs reused
         * @param index 引用在线性存储中的序号 / output slot index
         * @param ...args reset/构造函数 的参数列表 / reset or constructor arguments
         * @return 对象引用 / reference to the obtained element
         * @par Original Comment:
         * 尝试获取下一个对象，存在空闲对象使用next_free否则使用next
         */
        template<class... Ts> inline T& try_next_with_index(bool & flag,size_t& index,Ts&&... args){
            if(free_elements.empty()){
                flag = true;
                index = data.size();
                return next(std::forward<Ts>(args)...);
            }else {
                flag = false;
                return next_free_with_index(index,std::forward<Ts>(args)...);
            }
        }
    };
}


#endif
