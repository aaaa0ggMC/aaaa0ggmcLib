/**
 * @file adata_kernel.h
 * @brief Core Data Structures for ALib5 / ALib5 核心数据结构
 *
 * @details
 * English: This file contains the core dynamic data structures for ALib5, including dynamic value types 
 * and tree-like generic data nodes (similar to JSON). It provides memory-resource-aware classes (`std::pmr`) 
 * for building, traversing, modifying, merging, and diffing data trees.
 * 
 * Chinese: 本文件包含 ALib5 的核心动态数据结构，包括动态值类型和树状通用数据节点（类似 JSON）。
 * 它提供了基于内存资源 (`std::pmr`) 的类，用于构建、遍历、修改、合并和对比数据树。
 */

#ifndef ALIB5_ADATA_KERNEL
#define ALIB5_ADATA_KERNEL

#include <alib5/autil.h>
#include <alib5/adebug.h>
#include <alib5/aref.h>
#include <alib5/ecs/linear_storage.h>
#include <deque>
#include <variant>
#include <optional>
#include <charconv>
#include <algorithm>
#include <cmath>
#include <limits>

namespace alib5 {

    /**
     * @brief Configuration for array auto-expansion.
     * 
     * @par Original Comments:
     * English: Array access automatic expansion.
     * Chinese: 数组访问自动扩展
     */
    static constexpr size_t conf_array_auto_expand = 4;

    /**
     * @brief Epsilon used for floating-point comparisons.
     */
    static constexpr double conf_value_compare_epsilon = 1e-12;

    /**
     * @brief Concept defining valid types for node values.
     */
    template<class T> 
    concept IsNodeValue = 
        std::is_integral_v<std::decay_t<T>> || 
        std::is_floating_point_v<std::decay_t<T>> ||
        std::is_same_v<std::decay_t<T>, bool> || 
        IsStringLike<std::decay_t<T>> || 
        IsStringLike<T>;

    /**
     * @brief Concept defining a data parsing and dumping policy.
     * 
     * @par Original Comments:
     * English: At least requires dumping to pmr::string.
     * Chinese: 至少要求dump到pmr::string
     */
    template<class T, class Data> 
    concept IsDataPolicy = requires(T & t, std::pmr::string & dmp, std::string_view data, Data & d, const Data & cd) {
        t.parse(data, d);
        t.dump(dmp, cd);
    };

    /**
     * @brief Defines operations for merging data nodes.
     */
    enum class MergeOperation : int64_t {
        Override,
        Skip
    };

    /**
     * @brief Concept defining a merge function signature.
     * 
     * @par Original Comments:
     * English: Supports modifying the dest node (therefore requires this to be a safe BFS/DFS, beware of invalid references).
     * Chinese: 支持修改dest node (因此也就要求我这个一定要是安全BFS/DFS,小心引用失效)
     */
    template<class T, class Data> 
    concept IsMergeFn = requires(T& t, Data& dest, const Data& src) {
        { t(dest, src) } -> std::convertible_to<MergeOperation>;
    };

    /**
     * @brief Concept defining a diff function signature.
     */
    template<class T, class Data> 
    concept IsDiffFn = requires(T& t, const Data& dest, const Data& src) {
        { t(dest, src) } -> std::convertible_to<MergeOperation>;
    };

    /**
     * @brief Concept defining a prune function signature.
     */
    template<class T, class Data> 
    concept IsPruneFn = requires(T& t, const Data& dest) {
        { t(dest) } -> std::convertible_to<bool>;
    };

    /**
     * @brief Defines the fuzziness level for value comparisons.
     * 
     * @par Original Comments:
     * English: The fuzziness of the comparison gradually expands.
     * Chinese: 比较的模糊度逐渐扩大
     */
    enum class CompareStrategy : int64_t {
        Strict,     ///< Strictly restrict CacheValue type and exact numerical value.
        BoolStrict, ///< Allows INT and DOUBLE comparisons; Bool only supports 0 or 1.
        Lesser,     ///< Simulates C/C++ implicit boolean conversions.
        Fuzzy       ///< Allows cross-type comparisons (INT, STRING, etc.) if numerical values match.
    };

    namespace data {
        class JSON; ///< Forward declaration for default data policy.
    }

    /**
     * @brief Dynamic value wrapper capable of storing strings, integers, floats, or booleans.
     * 
     * @warning Not thread-safe. Must be locked under multi-threading even for const operations.
     * 
     * @par Original Comments:
     * English: High probability of malfunctioning in multi-threading, so locks must be added in multi-threaded environments!!! Regardless of const!!!
     * Chinese: 多线程下大概率不能正常工作,因此多线程下必须加锁!!!无论有没有const!!!
     */
    struct ALIB5_API CacheValue {            
        enum Type {
            STRING,
            INT,
            FLOATING,
            BOOL
        };

    private:
        mutable std::pmr::string data;
        mutable bool data_dirt;
        Type type;

        union {
            int64_t integer;
            double floating;
            bool boolean;
        };

        /**
         * @brief Marks the internal string data as dirty.
         */
        inline void mark() const { data_dirt = true; }

        /**
         * @brief Synchronizes the internal string data with a provided view.
         */
        inline void back_sync(std::string_view d) const {
            [[likely]] if(data_dirt) {
                data_dirt = false;
                this->data = d;
            }
        }

        /**
         * @brief Synchronizes internal numeric/boolean state to string representation.
         */
        void sync_to_string() const;

    public:
        /**
         * @brief Default constructor.
         * @param __a Memory resource allocator.
         */
        CacheValue(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : data(__a) {
            type = STRING;
            data_dirt = false;
            integer = 0;
        }

        /**
         * @brief Generic constructor for supported node values (non-string).
         */
        template<class T> 
        requires(!IsStringLike<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, CacheValue>)
        CacheValue(T&& d, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : data(__a) {
            type = STRING;
            data_dirt = true;
            transform<std::decay_t<T>>() = std::forward<T>(d);
        }

        /**
         * @brief String-specific constructor.
         */
        template<IsStringLike STR>
        CacheValue(STR&& d, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        : data(std::forward<STR>(d), __a) {
            type = STRING;
            data_dirt = false;
            integer = 0;
        }

        CacheValue(const CacheValue& other, std::pmr::memory_resource* a) 
        : data(other.data, a), data_dirt(other.data_dirt), type(other.type) {
            this->integer = other.integer; 
        }

        CacheValue(CacheValue&& other) ALIB5_NOEXCEPT 
        : data(std::move(other.data)), data_dirt(other.data_dirt), type(other.type), integer(other.integer) {
            other.data_dirt = true;
            other.data.clear();
        }

        CacheValue(CacheValue&& other, std::pmr::memory_resource* a) 
        : data(std::move(other.data), a), data_dirt(other.data_dirt), type(other.type) {
            this->integer = other.integer;
            if(a == other.data.get_allocator()) {
                other.data_dirt = true;
                other.data.clear();
            }
        }

        CacheValue& operator=(const CacheValue& other) {
            if(this == &other) [[unlikely]] return *this;
            data = other.data; 
            data_dirt = other.data_dirt;
            type = other.type;
            integer = other.integer;
            return *this;
        }

        CacheValue& operator=(CacheValue&& other) ALIB5_NOEXCEPT {
            if(this == &other) [[unlikely]] return *this;
            data = std::move(other.data);
            data_dirt = other.data_dirt;
            type = other.type;
            integer = other.integer;
            return *this;
        }

        /**
         * @brief Gets the current underlying data type.
         */
        inline Type get_type() const { return type; }

        /**
         * @brief Returns raw underlying string view representation (may be out of sync if dirty).
         */
        std::string_view raw_view() const { return data; }

        /**
         * @brief Transforms the underlying type and provides a reference for modification.
         * 
         * @tparam T The target type to transform into.
         * @tparam invoke_err Whether to report errors upon failure.
         * 
         * @par Original Comments:
         * English: Requires implementing modification logic. Requires ensuring short-term reference usage. Indicates whether to report an error here.
         * Chinese: 需要实现修改逻辑。需要确保使用短期引用。这里表示是否上报error。
         */
        template<class T, bool invoke_err = true> 
        auto& transform();

        /**
         * @brief Forcibly reconstructs the variable to a target type.
         * 
         * @par Original Comments:
         * English: Note that data is not cleared, so internal garbage values are kept.
         * Chinese: 注意数据没有被清除,因此内部保存有垃圾值。
         */
        template<class T> 
        auto& reconstruct();

        /**
         * @brief Sets the internal value directly.
         */
        template<class T> 
        CacheValue& set(T&& val);

        /**
         * @brief Assignment operator for supported node values.
         */
        template<IsNodeValue T> 
        inline CacheValue& operator=(T&& val) { 
            set(std::forward<T>(val)); 
            return *this; 
        }

        /**
         * @brief Validates type formatting without changing internal type.
         * @return std::pair<Result, Boolean> where boolean indicates parsing success.
         */
        template<class T> 
        auto expect() const;

        /**
         * @brief Casts the internal value to the specified type without altering state.
         * @throws May throw/panic if conversion fails.
         */
        template<class T> 
        auto to() const;

        /**
         * @brief Compares two values based on a given strategy.
         */
        static bool equals(const CacheValue& left, const CacheValue& right, CompareStrategy strategy = CompareStrategy::Strict);

        /**
         * @brief Compares this value against another.
         */
        bool equals(const CacheValue& b, CompareStrategy strategy = CompareStrategy::Strict) const {
            return equals(*this, b, strategy);
        }
    };

    /**
     * @brief Value policy whose const operations never mutate object state.
     *
     * Numeric stringify uses a thread-local scratch buffer, so returned string views are short-lived
     * and may be invalidated by the next stringify call on the same thread.
     */
    struct ALIB5_API ConstableValue {
        enum Type {
            STRING = CacheValue::STRING,
            INT = CacheValue::INT,
            FLOATING = CacheValue::FLOATING,
            BOOL = CacheValue::BOOL
        };

    private:
        std::pmr::string data;
        Type type;

        union {
            int64_t integer;
            double floating;
            bool boolean;
        };

        static std::pmr::string& stringify_buffer();
        std::string_view stringify() const;

    public:
        ConstableValue(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : data(__a) {
            type = STRING;
            integer = 0;
        }

        template<class T>
        requires(!IsStringLike<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, ConstableValue>)
        ConstableValue(T&& d, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : data(__a) {
            type = STRING;
            integer = 0;
            set(std::forward<T>(d));
        }

        template<IsStringLike STR>
        ConstableValue(STR&& d, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        : data(std::forward<STR>(d), __a) {
            type = STRING;
            integer = 0;
        }

        ConstableValue(const ConstableValue& other, std::pmr::memory_resource* a)
        : data(other.data, a), type(other.type) {
            this->integer = other.integer;
        }

        ConstableValue(ConstableValue&& other) ALIB5_NOEXCEPT
        : data(std::move(other.data)), type(other.type), integer(other.integer) {}

        ConstableValue(ConstableValue&& other, std::pmr::memory_resource* a)
        : data(std::move(other.data), a), type(other.type), integer(other.integer) {}

        ConstableValue& operator=(const ConstableValue& other) {
            if(this == &other) [[unlikely]] return *this;
            data = other.data;
            type = other.type;
            integer = other.integer;
            return *this;
        }

        ConstableValue& operator=(ConstableValue&& other) ALIB5_NOEXCEPT {
            if(this == &other) [[unlikely]] return *this;
            data = std::move(other.data);
            type = other.type;
            integer = other.integer;
            return *this;
        }

        inline Type get_type() const { return type; }
        std::string_view raw_view() const { return data; }

        template<class T, bool invoke_err = true>
        auto& transform();

        template<class T>
        auto& reconstruct();

        template<class T>
        ConstableValue& set(T&& val);

        template<IsNodeValue T>
        inline ConstableValue& operator=(T&& val) {
            set(std::forward<T>(val));
            return *this;
        }

        template<class T>
        auto expect() const;

        template<class T>
        auto to() const;

        static bool equals(const ConstableValue& left, const ConstableValue& right, CompareStrategy strategy = CompareStrategy::Strict);

        bool equals(const ConstableValue& b, CompareStrategy strategy = CompareStrategy::Strict) const {
            return equals(*this, b, strategy);
        }
    };

    using Value = CacheValue;

    /**
     * @brief A generic dynamic data node representing Null, CacheValue, Object, or Array.
     * 
     * @warning Not thread-safe regardless of const qualification.
     */
    template<class ValueType = CacheValue>
    struct ALIB5_API BasicAData {
        using value_type = ValueType;
        using data_type = BasicAData<value_type>;

        /**
         * @brief Object representation containing Key-CacheValue mappings.
         */
        struct ALIB5_API Object {
            using container_t = ecs::detail::LinearStorage<data_type>;
            
            container_t children; ///< Storage for child nodes.
            
            std::pmr::unordered_map<
                std::pmr::string,
                size_t,
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > object_mapper; ///< Key to index mapping.
        
            Object(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE);
            
            Object(const Object& other, std::pmr::memory_resource* a)
            : children(other.children, a), object_mapper(other.object_mapper, a) {}

            Object(Object&& other) ALIB5_NOEXCEPT
            : children(std::move(other.children)), object_mapper(std::move(other.object_mapper)) {}

            Object(Object&& other, std::pmr::memory_resource* a)
            : children(std::move(other.children), a), object_mapper(std::move(other.object_mapper), a) {}

            Object& operator=(const Object& other) {
                if(this == &other) [[unlikely]] return *this;
                children = other.children;
                object_mapper = other.object_mapper;
                return *this;
            }

            Object& operator=(Object&& other) ALIB5_NOEXCEPT {
                if(this == &other) [[unlikely]] return *this;
                children = std::move(other.children);
                object_mapper = std::move(other.object_mapper);
                return *this;
            }

            void reserve(size_t buffer_size) {
                children.reserve(buffer_size);
            }
            
            /**
             * @brief Ensures a node exists for a given key, creating it if necessary.
             * 
             * @par Original Comments:
             * English: Object has strict requirements for index alignment, so multithreaded operations must be locked by the user.
             * Chinese: Object对索引对齐要求十分严格,因此多线程操作一定要自己加锁
             */
            std::pair<data_type*, size_t> ALIB5_API ensure_node(std::string_view key);
            
            bool ALIB5_API rename(std::string_view old_name, std::string_view new_name);
            bool ALIB5_API remove(std::string_view name);

            /**
             * @brief Safely visits a node by wrapping it in a Reference wrapper.
             */
            inline RefWrapper<container_t> safe_visit(std::string_view visit) { 
                return ref(children, ensure_node(visit).second); 
            }

            inline data_type& operator[](std::string_view visit) { 
                return *ensure_node(visit).first; 
            }
        
            inline const data_type* at_ptr(std::string_view visit) const {
                auto it = object_mapper.find(visit);
                if(it == object_mapper.end()) return nullptr;
                return &children.data[it->second];
            }
            
            inline data_type* at_ptr(std::string_view visit) {
                return const_cast<data_type*>(
                    (((const data_type::Object*)(this))->at_ptr(visit))
                );
            }

            inline void clear() {
                children.clear();
                object_mapper.clear();
            }

            /**
             * @brief Const access to a node. Will panic if the node does not exist.
             */
            inline const data_type& operator[](std::string_view visit) const {
                auto a = at_ptr(visit);
                panicf_if(!a, "Invalid visit {}!", visit);
                return *a;
            }

            inline size_t size() const { return object_mapper.size(); }
            inline bool empty() const { return !object_mapper.size(); }
            inline bool contains(std::string_view key) const {
                return object_mapper.find(key) != object_mapper.end();
            }

            template<bool is_const>
            struct ObjectIterator {
                using iterator_category = std::forward_iterator_tag;
                using value_type = data_type;
                using difference_type = std::ptrdiff_t;
                using pointer = std::conditional_t<is_const, const value_type*, value_type*>;
                using reference = std::conditional_t<is_const, const value_type&, value_type&>;
                
                using it_type = std::conditional_t<is_const,
                    typename decltype(object_mapper)::const_iterator,
                    typename decltype(object_mapper)::iterator
                >;
                using cont_ref = std::conditional_t<is_const, const container_t&, container_t&>;
                
                it_type it;
                cont_ref cont;

                struct Proxy {
                    const std::pmr::string& _first;
                    reference _second;

                    auto& first() const { return _first; }
                    reference second() const { return _second; }
                };

                ObjectIterator(it_type iter, cont_ref container) : it(iter), cont(container) {}

                auto& first() const { return it->first; }
                auto& second() const { return cont.data[it->second]; }

                auto operator*() const { return Proxy{it->first, cont.data[it->second]}; }
                auto operator++() { return ++it; }
                auto operator++(int) { return it++; }
                bool operator==(const ObjectIterator& other) const { return it == other.it; }
                bool operator!=(const ObjectIterator& other) const { return it != other.it; }
                auto operator->() const { return Proxy{it->first, cont.data[it->second]}; } 
            };

            using iterator = ObjectIterator<false>;
            using const_iterator = ObjectIterator<true>;
            using value_type = decltype(children.data)::value_type;

            iterator begin() { return {object_mapper.begin(), children}; }
            const_iterator begin() const { return {object_mapper.begin(), children}; }
            iterator end() { return {object_mapper.end(), children}; }
            const_iterator end() const { return {object_mapper.end(), children}; }
        
            iterator find(std::string_view d) { return {object_mapper.find(d), children}; }
            const_iterator find(std::string_view d) const { return {object_mapper.find(d), children}; }
        };

        /**
         * @brief Array representation containing an ordered list of BasicAData nodes.
         */
        struct ALIB5_API Array {
            using container_t = std::pmr::vector<data_type>;
            container_t values;

            ALIB5_API Array(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE);
            
            Array(const Array& other, std::pmr::memory_resource* a) : values(other.values, a) {}
            Array(Array&& other) ALIB5_NOEXCEPT : values(std::move(other.values)) {}
            Array(Array&& other, std::pmr::memory_resource* a) : values(std::move(other.values), a) {}

            Array& operator=(const Array& other) {
                if(this == &other) [[unlikely]] return *this;
                values = other.values;
                return *this;
            }

            Array& operator=(Array&& other) ALIB5_NOEXCEPT {
                if(this == &other) [[unlikely]] return *this;
                values = std::move(other.values);
                return *this;
            }

            /**
             * @brief Safe visitation logic. Cannot survive shrink operations.
             */
            RefWrapper<container_t> safe_visit(std::ptrdiff_t index);
            
            data_type& operator[](std::ptrdiff_t index);
            const data_type& operator[](std::ptrdiff_t index) const;

            void reserve(size_t size) { values.reserve(size); }
            
            /**
             * @brief Ensures array length and returns span of newly created elements.
             */
            std::span<data_type> ALIB5_API ensure(size_t size);

            inline size_t size() const { return values.size(); }
            inline void clear() { values.clear(); }
            inline bool empty() const { return values.empty(); }

            inline const data_type* at_ptr(ptrdiff_t index) const {
                if(index < 0) index = values.size() + index;
                if(index >= values.size()) return nullptr;
                return &values[index];
            } 
            
            inline data_type* at_ptr(ptrdiff_t index) {
                return const_cast<data_type*>(
                    (((const Array*)(this))->at_ptr(index))
                );
            }

            auto begin() { return values.begin(); }
            auto begin() const { return values.begin(); }
            auto end() { return values.end(); }
            auto end() const { return values.end(); }
        };

        enum Type {
            TNull,
            TValue,
            TObject,
            TArray
        };

    private:
        std::variant<std::monostate, value_type, Object, Array> data;
        std::pmr::memory_resource* allocator;
        
        template<class T> 
        auto& __get_value() const {
            using type = std::decay_t<T>;
            const type* f = std::get_if<type>(&data);
            [[likely]] if(f) {
                return *f;
            }
            vpanicf_if(!f,
                "Type doesnt match! CurrentId:{} (0 null, 1 value, 2 object, 3 array), RequestedType:{}",
                (int)get_type(), typeid(type).name());
            std::abort();
        }

        template<class T> 
        inline auto& __get_value() {
            return const_cast<std::decay_t<T>&>(
                static_cast<const BasicAData*>(this)->__get_value<T>()
            );
        }

        template<class T> 
        auto& __ensure_type() {
            using type = std::decay_t<T>;
            if(std::holds_alternative<std::monostate>(data)) {
                return set<T>();
            } else [[likely]] {
                return __get_value<T>();
            }
        }

        static auto clone_data(const decltype(data)& src, std::pmr::memory_resource* res) {
            return std::visit([res](auto&& v) -> decltype(data) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) return v;
                else if constexpr (std::is_same_v<T, value_type>) {
                    value_type x(res);
                    x = v;
                    return x; 
                } else if constexpr (std::is_same_v<T, Object>) {
                    Object o(res);
                    o = v;
                    return o;
                } else {
                    Array a(res);
                    a = v;
                    return a;
                }
            }, src);
        }

    public:
        inline std::pmr::memory_resource* get_allocator() { return allocator; }

        template<class T> T& set();
        auto& set_null() { return set<std::monostate>(); }

        explicit BasicAData(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) {
            allocator = __a;
        }

        template<IsNodeValue T> 
        requires (!std::is_same_v<std::remove_cvref_t<T>, BasicAData>)
        BasicAData(T&& val, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) {
            allocator = __a;
            this->operator=(std::forward<T>(val));   
        }

        BasicAData(const BasicAData& other)
        : allocator(other.allocator), data(clone_data(other.data, other.allocator)) {}
        
        BasicAData(BasicAData&& other) ALIB5_NOEXCEPT : allocator(other.allocator) {
            *this = std::move(other);
        }

        BasicAData(const BasicAData& other, std::pmr::memory_resource* __a) : allocator(__a) {
            data = clone_data(other.data, __a);
        }

        BasicAData(const BasicAData::Object& other, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : allocator(__a) {
            set<BasicAData::Object>();
            object() = other;
        }

        BasicAData(const BasicAData::Array& other, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : allocator(__a) {
            set<BasicAData::Array>();
            array() = other;
        }

        BasicAData(const value_type& other, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : allocator(__a) {
            set<value_type>();
            value() = other;
        }

        BasicAData(const std::monostate& other, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) : allocator(__a) {
            set<std::monostate>();
        }

        ~BasicAData() = default;

        /**
         * @brief Returns a detached copy of this data.
         * 
         * @par Original Comments:
         * English: Solves the crash caused by assignment between parent and child. Issue resolved, API retained for compatibility.
         * Chinese: 解决父子之间赋值出现的crash. 问题已经被处理了,调整了赋值链条,但是api保留一下
         */
        BasicAData detach() const { return *this; }

        inline Type get_type() const { return (Type)data.index(); }
        inline bool is_null() const { return get_type() == TNull; }
        inline bool is_object() const { return get_type() == TObject; }
        inline bool is_array() const { return get_type() == TArray; }
        inline bool is_value() const { return get_type() == TValue; }

        static bool equals(const BasicAData& left, const BasicAData& right, CompareStrategy strategy = CompareStrategy::Strict);
        bool equals(const BasicAData& b, CompareStrategy strategy = CompareStrategy::Strict) const {
            return equals(*this, b, strategy);
        }

        value_type& value() { return __get_value<value_type>(); }
        Object& object() { return __get_value<Object>(); }
        Array& array() { return __get_value<Array>(); }
        std::monostate& null() { return __get_value<std::monostate>(); }
        
        const value_type& value() const { return __get_value<value_type>(); }
        const Object& object() const { return __get_value<Object>(); }
        const Array& array() const { return __get_value<Array>(); }
        const std::monostate& null() const { return __get_value<std::monostate>(); }

        template<IsNodeValue T> 
        BasicAData& operator=(T&& val) {
            value_type& v = __ensure_type<value_type>();
            v = std::forward<T>(val);
            return *this;
        }

        template<class T> 
        BasicAData& rewrite(T&& val) {
            set<std::monostate>();
            *this = std::forward<T>(val);
            return *this;
        }

        /**
         * @brief Rewrites current node utilizing move semantics on another BasicAData node. 
         * The moved object gets reset to NULL.
         */
        BasicAData& rewrite(BasicAData&& other) {
            using safe_t = std::remove_cv_t<decltype(data)>;
            if(this == &other) return *this;
            safe_t d = std::move(other.data);
            other.set<std::monostate>();

            if(this->allocator == other.allocator) {
                this->data = std::move(d);
            } else {
                this->data = clone_data(d, allocator);
            }
            return *this;
        }

        BasicAData(std::initializer_list<const BasicAData> list) {
            *this = list;
        }

        BasicAData& operator=(std::initializer_list<const BasicAData> list) {
            [[unlikely]] if(!is_array() && !is_null()) {
                panic_if(!is_array() && !is_null(), "Implicit type cast is forbidden.");
            }
            auto& arr = this->set<Array>();
            for(auto&& item : list) {
                arr.values.emplace_back(std::forward<decltype(item)>(item), this->allocator);
            }
            return *this;
        }

        BasicAData& operator=(BasicAData&& val) {
            rewrite(std::move(val));    
            return *this;
        }

        BasicAData& operator=(const BasicAData& val) {
            [[unlikely]] if(this == &val) return *this;
            [[unlikely]] if(get_type() != TNull && val.get_type() != TNull && get_type() != val.get_type()) {
                panic_if(get_type() != val.get_type(), "Implicit type cast is forbidden.");
                std::abort();
            }
            
            if(val.allocator == this->allocator) {
                decltype(data) safe_d = clone_data(val.data, allocator);
                data = std::move(safe_d);
            } else {
                this->data = clone_data(val.data, allocator);
            }
            return *this;
        }

        BasicAData& operator[](std::string_view visit) {
            Object& o = __ensure_type<Object>();
            return o[visit];
        }

        const BasicAData& operator[](std::string_view visit) const {
            return object()[visit];
        }

        BasicAData& operator[](std::ptrdiff_t visit) {
            Array& o = __ensure_type<Array>();
            return o[visit];
        }

        const BasicAData& operator[](std::ptrdiff_t visit) const {
            return array()[visit];
        }

        template<class T> 
        auto to() const {
            return value().template to<T>();
        }

        template<class T> 
        auto try_to() -> std::optional<decltype(to<T>())> const {
            if(get_type() != TValue) return std::nullopt;
            return std::optional(to<T>());
        }

        template<class T> 
        auto expect() const {
            return value().template expect<T>();
        }

        template<class T> 
        auto try_expect() -> std::optional<decltype(expect<T>())> const {
            if(get_type() != TValue) return std::nullopt;
            return std::optional(expect<T>());
        }

        /**
         * @brief Jumps to a specific pointer path. Returns nullptr if missing.
         */
        const BasicAData* ALIB5_API jump_ptr(std::string_view path, bool invoke_err = true) const;

        BasicAData* jump_ptr(std::string_view path, bool invoke_err = true) {
            return const_cast<BasicAData*>(
                (((const BasicAData*)this)->jump_ptr(path, invoke_err))
            );
        }

        BasicAData& jump(std::string_view path, bool invoke_err = true) {
            BasicAData* ptr = jump_ptr(path, invoke_err);
            [[unlikely]] if(!ptr) {
                panicf_if(ptr == NULL, "Failed to locate object with path {}!", path);
            }
            return *ptr; 
        }

        const BasicAData& jump(std::string_view path, bool invoke_err = true) const {
            const BasicAData* ptr = jump_ptr(path, invoke_err);
            [[unlikely]] if(!ptr) {
                panicf_if(ptr == NULL, "Failed to locate object with path {}!", path);
            }
            return *ptr; 
        }

        static inline MergeOperation __merge__default(BasicAData& dest, const BasicAData& src) {
            return MergeOperation::Override;
        }

        static inline MergeOperation __diff__default(const BasicAData& dest, const BasicAData& src) {
            return MergeOperation::Override;
        }

        static inline bool __prune__default(const BasicAData& d) {
            if(d.is_null()) return true;
            else if(d.is_array() && d.array().size() == 0) return true;
            else if(d.is_object() && d.object().size() == 0) return true;
            return false;
        }
        
        template<bool PruneArray = false, IsPruneFn<BasicAData> EmptyFn = decltype(__prune__default)>
        BasicAData& prune(EmptyFn&& fn = __prune__default);

        template<class Other, class MergeFn>
        BasicAData& __merge_impl(Other&& in, MergeFn&& fn);

        template<IsMergeFn<BasicAData> MergeFn = decltype(__merge__default)>
        inline BasicAData& merge(const BasicAData& in, MergeFn&& fn = __merge__default) {
            return __merge_impl<const BasicAData&>(in, std::forward<MergeFn>(fn));
        }

        /**
         * @brief Fast r-value merge logic.
         * 
         * @par Original Comments:
         * English: Fast r-value processing.
         * Chinese: 快速右值
         */
        template<IsMergeFn<BasicAData> MergeFn = decltype(__merge__default)>
        inline BasicAData& merge(BasicAData&& in, MergeFn&& fn = __merge__default) {
            return __merge_impl<BasicAData&&>(std::move(in), std::forward<MergeFn>(fn));
        }

        template<IsDiffFn<BasicAData> DiffFn = decltype(__diff__default)>
        inline bool diff(
            const BasicAData& in,
            BasicAData* added_or_modified = nullptr,
            BasicAData* src_lack_of = nullptr,
            DiffFn&& fn = __diff__default
        ) const;

        template<IsDataPolicy<BasicAData> DataPolicy = data::JSON> 
        auto load_from_memory(std::string_view data, DataPolicy&& parser = DataPolicy()) {
            return std::forward<DataPolicy>(parser).parse(data, *this);
        }

        template<IsDataPolicy<BasicAData> DataPolicy = data::JSON> 
        auto load_from_entry(io::FileEntry entry, DataPolicy&& parser = DataPolicy()) {
            return load_from_memory(entry.read(), std::forward<DataPolicy>(parser));
        }

        template<IsDataPolicy<BasicAData> DataPolicy = data::JSON> 
        auto load_from_file(std::string_view path, DataPolicy&& parser = DataPolicy()) {
            return load_from_entry(io::load_entry(path, false), std::forward<DataPolicy>(parser));
        }

        template<IsDataPolicy<BasicAData> Dumper = data::JSON, class T> 
        auto dump(T& target, Dumper&& dumper = Dumper()) const {
            return std::forward<Dumper>(dumper).dump(target, *this);
        }

        template<IsDataPolicy<BasicAData> Dumper = data::JSON> 
        auto dump_to_string(Dumper&& dumper = Dumper(), std::pmr::memory_resource* res = ALIB5_DEFAULT_MEMORY_RESOURCE) const {
            std::pmr::string str(res);
            dump(str, std::forward<Dumper>(dumper));
            return str;
        }

        template<IsDataPolicy<BasicAData> Dumper = data::JSON> 
        auto dump_to_entry(const io::FileEntry& entry, Dumper&& dumper = Dumper(), std::pmr::memory_resource* res = ALIB5_DEFAULT_MEMORY_RESOURCE) const {
            std::pmr::string str(res);
            auto val = dump(str, std::forward<Dumper>(dumper));
            entry.write_all(str);
            return val;
        }       

        template<IsDataPolicy<BasicAData> Dumper = data::JSON> 
        auto dump_to_file(std::string_view file_path, Dumper&& dumper = Dumper(), std::pmr::memory_resource* res = ALIB5_DEFAULT_MEMORY_RESOURCE) const {
            return dump_to_entry(io::load_entry(file_path, false), std::forward<Dumper>(dumper), res);
        }

        template<IsDataPolicy<BasicAData> Dumper = data::JSON>
        auto str(Dumper&& dmp = Dumper()) const {
            return dump_to_string(std::forward<Dumper>(dmp));
        }
        
        template<class STR, class Dumper = data::JSON> 
        void write_to_log(STR& s) const {
            dump(s, Dumper());
        }
    };

    template<class Dumper = data::JSON,class V = CacheValue>
    inline BasicAData<V> adata_from_memory(std::string_view mem, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE) {
        BasicAData<V> data(__a);
        data.template load_from_memory<Dumper>(mem);
        return data;
    }

    using AData = BasicAData<>;
    using dvalue_t = CacheValue;
    using dadata_t = AData;
    using dobject_t = AData::Object;
    using darray_t = AData::Array;
    using dtype_t = AData::Type;
    using dvalue_type_t = CacheValue::Type;

} // namespace alib5

// ----------------------------------------------------------------------------------------------------
// Inline Implementations (Moved to bottom due to length)
// ----------------------------------------------------------------------------------------------------

namespace alib5 {

    inline void CacheValue::sync_to_string() const {
        if(!data_dirt) return;
        switch(type) {
            case INT:      data = ext::to_string(integer); break;
            case FLOATING: data = ext::to_string(floating); break;
            case BOOL:     data = ext::to_string(boolean); break;
            default: break;
        }
        data_dirt = false;
    }

    template<class T> 
    inline auto& CacheValue::reconstruct() {
        if constexpr(IsStringLike<T>) {
            type = STRING;
            data_dirt = false;
            return data;
        } else {
            data_dirt = true; 
            
            if constexpr(std::is_same_v<T, bool>) {
                type = BOOL;
                return boolean;
            } else if constexpr(std::is_integral_v<T>) {
                type = INT;
                return integer;
            } else if constexpr(std::is_floating_point_v<T>) {
                type = FLOATING;
                return floating;
            }
        }
    }

    template<class TP, bool invoke_err> 
    inline auto& CacheValue::transform() {
        using T = std::remove_const_t<TP>;
        auto generator = [this]<class T1, class T2, class T3, Type t1, Type t2, Type t3>(T1& v1, T2& v2, T3& v3) -> auto& {
            [[likely]] if(type == t1) {
            } else if(type == t2) {
                v1 = (T1)v2;
            } else if(type == t3) {
                v1 = (T1)v3;
            } else if(type == STRING) {
                std::from_chars_result result {};
                v1 = ext::to_T<T1>(str::trim(data), &result);
                if constexpr(invoke_err) {
                    if(result.ec != std::errc()) {
                        invoke_error(err_format_error, "Failed to format \"{}\".", data);
                    }
                }
            }
            type = t1;
            data_dirt = true;
            return v1;
        };
        
        if constexpr(IsStringLike<T>) {
            sync_to_string();
            type = STRING;
            return data;
        } else if constexpr(std::is_same_v<T, bool>) {
            return generator.template operator()<bool, double, int64_t, BOOL, FLOATING, INT>(
                boolean, floating, integer
            );
        } else if constexpr(std::is_integral_v<T>) {
            return generator.template operator()<int64_t, double, bool, INT, FLOATING, BOOL>(
                integer, floating, boolean
            );
        } else if constexpr(std::is_floating_point_v<T>) {
            return generator.template operator()<double, int64_t, bool, FLOATING, INT, BOOL>(
                floating, integer, boolean
            );
        } else {
            static_assert(std::is_same_v<T, bool>, "Unsupported type!");
        }
    }

    template<class T> 
    inline auto CacheValue::expect() const {
        auto make_value = [this]<class T1>(T1& val) {
            if constexpr(IsStringLike<T>) {
                if(data_dirt) back_sync(ext::to_string(val));
                return std::make_pair(std::string_view(data), true);
            } else return std::make_pair(T(val), true);
        };

        switch(type) {
        case STRING: {
            std::from_chars_result result {};
            auto v = ext::to_T<T>(data, &result);
            return std::make_pair(v, result.ec == std::errc() && result.ptr == (data.data() + data.size()));
        }
        case INT:
            return make_value(integer);
        case FLOATING:
            return make_value(floating);
        default:
            return make_value(boolean);
        }
    }

    template<class T> 
    inline auto CacheValue::to() const {
        auto ret = expect<T>();
        if(!ret.second) {
            invoke_error(err_format_error, "Cannot format \"{}\" correctly!", data);
        }
        return ret.first;
    }

    template<class T> 
    inline CacheValue& CacheValue::set(T&& val) {
        auto& ref = reconstruct<std::decay_t<T>>();
        ref = std::forward<T>(val);
        return *this;
    }

    inline bool CacheValue::equals(const CacheValue& left, const CacheValue& right, CompareStrategy st) {
        auto float_equal = [](double a, double b) {
            if(a == b) return true;
            double diff = std::abs(a - b);
            if(diff < conf_value_compare_epsilon) return true;

            return diff <= ((std::abs(a) < std::abs(b) ? std::abs(b) : std::abs(a))
                * std::numeric_limits<double>::epsilon());
        };

        auto lt = left.get_type();
        auto rt = right.get_type();
        if(lt == rt) {
            switch(lt) {
            case Type::BOOL:
                return left.boolean == right.boolean;
            case Type::FLOATING:
                if(st == CompareStrategy::Strict) return left.floating == right.floating;
                return float_equal(left.floating, right.floating);
            case Type::INT:
                return left.integer == right.integer;
            case Type::STRING:
                return left.data == right.data;
            }
            panic_debug(true, "aaaa0ggmc,你这里忘记处理其他类型了!!!同时注意下面的代码也要同步你的类型更改");
        }

        if(st == CompareStrategy::Strict) return false;

        auto is_numeric = [](Type t) {
            return t == Type::FLOATING || t == Type::INT || t == Type::BOOL;
        };

        if(is_numeric(lt) && is_numeric(rt)) {
            if(lt == Type::BOOL || rt == Type::BOOL) {
                if(st == CompareStrategy::BoolStrict) {
                    return float_equal(left.to<double>(), right.to<double>());
                }
                return left.to<bool>() == right.to<bool>();
            }
            return right.to<double>() == left.to<double>();
        }

        if(st == CompareStrategy::BoolStrict || st == CompareStrategy::Lesser) return false;

        auto& plt = (lt == Type::STRING) ? right : left;
        auto& prt = (lt == Type::STRING) ? left : right;
        lt = plt.get_type();

        if(lt == Type::FLOATING) {
            auto db = prt.expect<double>();
            if(!db.second) return false;
            return float_equal(db.first, plt.floating);
        } else if(lt == Type::INT) {
            auto db = prt.expect<double>();
            if(!db.second) return false;
            return float_equal(db.first, static_cast<double>(plt.integer));
        } else if(lt == Type::BOOL) {
            std::string_view s = prt.data;
            if(plt.boolean) {
                return s == "true" || s == "TRUE" || s == "1" || s == "yes" || s == "YES";
            }
            return s == "false" || s == "FALSE" || s == "0" || s == "no" || s == "NO";
        }

        return false;
    }

    inline std::pmr::string& ConstableValue::stringify_buffer() {
        static thread_local std::pmr::string buffer(ALIB5_DEFAULT_MEMORY_RESOURCE);
        return buffer;
    }

    inline std::string_view ConstableValue::stringify() const {
        if(type == STRING) return data;

        auto& buffer = stringify_buffer();
        switch(type) {
        case INT:
            buffer = ext::to_string(integer);
            break;
        case FLOATING:
            buffer = ext::to_string(floating);
            break;
        case BOOL:
            buffer = ext::to_string(boolean);
            break;
        case STRING:
            break;
        }
        return std::string_view(buffer);
    }

    template<class T>
    inline auto& ConstableValue::reconstruct() {
        if constexpr(IsStringLike<T>) {
            type = STRING;
            return data;
        } else {
            data.clear();

            if constexpr(std::is_same_v<T, bool>) {
                type = BOOL;
                return boolean;
            } else if constexpr(std::is_integral_v<T>) {
                type = INT;
                return integer;
            } else if constexpr(std::is_floating_point_v<T>) {
                type = FLOATING;
                return floating;
            } else {
                static_assert(std::is_same_v<T, bool>, "Unsupported type!");
            }
        }
    }

    template<class TP, bool invoke_err>
    inline auto& ConstableValue::transform() {
        using T = std::remove_const_t<TP>;
        auto generator = [this]<class T1, class T2, class T3, Type t1, Type t2, Type t3>(T1& v1, T2& v2, T3& v3) -> auto& {
            [[likely]] if(type == t1) {
            } else if(type == t2) {
                v1 = static_cast<T1>(v2);
            } else if(type == t3) {
                v1 = static_cast<T1>(v3);
            } else if(type == STRING) {
                std::from_chars_result result {};
                v1 = ext::to_T<T1>(str::trim(data), &result);
                if constexpr(invoke_err) {
                    if(result.ec != std::errc()) {
                        invoke_error(err_format_error, "Failed to format \"{}\".", data);
                    }
                }
            }
            type = t1;
            data.clear();
            return v1;
        };

        if constexpr(IsStringLike<T>) {
            if(type != STRING) {
                data = stringify();
                type = STRING;
            }
            return data;
        } else if constexpr(std::is_same_v<T, bool>) {
            return generator.template operator()<bool, double, int64_t, BOOL, FLOATING, INT>(
                boolean, floating, integer
            );
        } else if constexpr(std::is_integral_v<T>) {
            return generator.template operator()<int64_t, double, bool, INT, FLOATING, BOOL>(
                integer, floating, boolean
            );
        } else if constexpr(std::is_floating_point_v<T>) {
            return generator.template operator()<double, int64_t, bool, FLOATING, INT, BOOL>(
                floating, integer, boolean
            );
        } else {
            static_assert(std::is_same_v<T, bool>, "Unsupported type!");
        }
    }

    template<class T>
    inline auto ConstableValue::expect() const {
        auto make_value = [this]<class T1>(const T1& val) {
            if constexpr(IsStringLike<T>) {
                auto& buffer = stringify_buffer();
                buffer = ext::to_string(val);
                return std::make_pair(std::string_view(buffer), true);
            } else {
                return std::make_pair(T(val), true);
            }
        };

        switch(type) {
        case STRING: {
            if constexpr(IsStringLike<T>) {
                return std::make_pair(std::string_view(data), true);
            } else {
                std::from_chars_result result {};
                auto v = ext::to_T<T>(data, &result);
                return std::make_pair(v, result.ec == std::errc() && result.ptr == (data.data() + data.size()));
            }
        }
        case INT:
            return make_value(integer);
        case FLOATING:
            return make_value(floating);
        default:
            return make_value(boolean);
        }
    }

    template<class T>
    inline auto ConstableValue::to() const {
        auto ret = expect<T>();
        if(!ret.second) {
            invoke_error(err_format_error, "Cannot format \"{}\" correctly!", stringify());
        }
        return ret.first;
    }

    template<class T>
    inline ConstableValue& ConstableValue::set(T&& val) {
        auto& ref = reconstruct<std::decay_t<T>>();
        ref = std::forward<T>(val);
        return *this;
    }

    inline bool ConstableValue::equals(const ConstableValue& left, const ConstableValue& right, CompareStrategy st) {
        auto float_equal = [](double a, double b) {
            if(a == b) return true;
            double diff = std::abs(a - b);
            if(diff < conf_value_compare_epsilon) return true;

            return diff <= ((std::abs(a) < std::abs(b) ? std::abs(b) : std::abs(a))
                * std::numeric_limits<double>::epsilon());
        };

        auto lt = left.get_type();
        auto rt = right.get_type();
        if(lt == rt) {
            switch(lt) {
            case Type::BOOL:
                return left.boolean == right.boolean;
            case Type::FLOATING:
                if(st == CompareStrategy::Strict) return left.floating == right.floating;
                return float_equal(left.floating, right.floating);
            case Type::INT:
                return left.integer == right.integer;
            case Type::STRING:
                return left.data == right.data;
            }
            panic_debug(true, "aaaa0ggmc,你这里忘记处理其他类型了!!!同时注意下面的代码也要同步你的类型更改");
        }

        if(st == CompareStrategy::Strict) return false;

        auto is_numeric = [](Type t) {
            return t == Type::FLOATING || t == Type::INT || t == Type::BOOL;
        };

        if(is_numeric(lt) && is_numeric(rt)) {
            if(lt == Type::BOOL || rt == Type::BOOL) {
                if(st == CompareStrategy::BoolStrict) {
                    return float_equal(left.to<double>(), right.to<double>());
                }
                return left.to<bool>() == right.to<bool>();
            }
            return right.to<double>() == left.to<double>();
        }

        if(st == CompareStrategy::BoolStrict || st == CompareStrategy::Lesser) return false;

        auto& plt = (lt == Type::STRING) ? right : left;
        auto& prt = (lt == Type::STRING) ? left : right;
        lt = plt.get_type();

        if(lt == Type::FLOATING) {
            auto db = prt.expect<double>();
            if(!db.second) return false;
            return float_equal(db.first, plt.floating);
        } else if(lt == Type::INT) {
            auto db = prt.expect<double>();
            if(!db.second) return false;
            return float_equal(db.first, static_cast<double>(plt.integer));
        } else if(lt == Type::BOOL) {
            std::string_view s = prt.data;
            if(plt.boolean) {
                return s == "true" || s == "TRUE" || s == "1" || s == "yes" || s == "YES";
            }
            return s == "false" || s == "FALSE" || s == "0" || s == "no" || s == "NO";
        }

        return false;
    }

    template<class V>
    inline BasicAData<V>::Object::Object(std::pmr::memory_resource* __a)
        : children(0, __a, __a)
        , object_mapper(__a) {}

    template<class V>
    inline std::pair<BasicAData<V>*, size_t> BasicAData<V>::Object::ensure_node(std::string_view key) {
        auto it = object_mapper.find(key);
        if(it != object_mapper.end()) {
            return std::make_pair(&children[it->second], it->second);
        }

        auto alloc = object_mapper.get_allocator();
        size_t index = 0;
        bool flag;
        BasicAData<V>& node = children.try_next_with_index(flag, index, alloc.resource());
        object_mapper.emplace(std::pmr::string(key, alloc), index);
        return std::make_pair(&node, index);
    }

    template<class V>
    inline bool BasicAData<V>::Object::remove(std::string_view name) {
        auto it = object_mapper.find(name);
        if(it == object_mapper.end()) {
            return false;
        }
        children.remove(it->second);
        object_mapper.erase(it);
        return true;
    }

    template<class V>
    inline bool BasicAData<V>::Object::rename(std::string_view old_name, std::string_view new_name) {
        auto it = object_mapper.find(old_name);
        if(it == object_mapper.end()) {
            return false;
        }
        if(old_name == new_name) {
            return true;
        }
        auto index = it->second;
        object_mapper.erase(it);
        object_mapper.emplace(
            std::pmr::string(new_name, object_mapper.get_allocator()),
            index
        );
        return true;
    }

    template<class V>
    inline BasicAData<V>::Array::Array(std::pmr::memory_resource* __a)
        : values(__a) {}

    template<class V>
    inline std::span<BasicAData<V>> BasicAData<V>::Array::ensure(size_t size) {
        if(size <= values.size()) return {};
        auto res = values.get_allocator().resource();
        auto beg = values.size();

        values.resize(size, BasicAData<V>(res));

        return std::span(values.begin() + beg, values.end());
    }

    template<class V>
    inline const BasicAData<V>* BasicAData<V>::jump_ptr(std::string_view path, bool err) const {
        if(path.empty()) return this;
        if(path[0] != '/') {
            if(err) invoke_error(err_locate_error, "Failed to parse pointer which isn't begin with '/'!PATH:{}", path);
            return nullptr;
        }

        auto splits = str::split(path, '/');
        std::span<std::string_view> main = splits;
        main = main.subspan(1);

        const BasicAData<V>* current = this;
        int index = 0;
        value_type val(allocator);

        while(index < main.size()) {
            std::string_view s = main[index];
            ++index;

            val = s;

            if(auto ss = val.template expect<int>(); ss.second && current->is_array()) {
                auto* ptr = current->array().at_ptr(ss.first);
                if(ptr) {
                    current = ptr;
                } else {
                    if(err) {
                        invoke_error(err_locate_error, "Locate failed when finding array index {}!", ss.first);
                    }
                    return nullptr;
                }
            } else if(current->is_object()) {
                static thread_local std::string cast = "";
                cast.clear();
                for(size_t i = 0; i < s.size(); ++i) {
                    if(s[i] == '~' && i + 1 < s.size()) {
                        if(s[i + 1] == '0') {
                            cast.push_back('~');
                            ++i;
                            continue;
                        } else if(s[i + 1] == '1') {
                            cast.push_back('/');
                            ++i;
                            continue;
                        }
                    }
                    cast.push_back(s[i]);
                }
                auto* ptr = current->object().at_ptr(cast);
                if(ptr) {
                    current = ptr;
                } else {
                    if(err) {
                        invoke_error(err_locate_error, "Locate failed when finding object name {}!", cast);
                    }
                    return nullptr;
                }
            } else {
                if(err) {
                    invoke_error(err_locate_error, "Locate failed for the lack of depth!");
                }
                return nullptr;
            }
        }
        return current;
    }

    template<class V>
    inline bool BasicAData<V>::equals(const BasicAData<V>& left, const BasicAData<V>& right, CompareStrategy st) {
        struct Frame {
            const BasicAData<V>* left;
            const BasicAData<V>* right;
        };
        std::deque<Frame> frames;
        frames.emplace_back(&left, &right);

        while(!frames.empty()) {
            Frame f = frames.back();
            frames.pop_back();

            auto lt = f.left->get_type();
            auto rt = f.right->get_type();
            if(lt != rt) {
                return false;
            }

            switch(lt) {
            case Type::TValue:
                if(!f.left->value().equals(f.right->value(), st)) return false;
                break;
            case Type::TArray: {
                auto& la = f.left->array();
                auto& ra = f.right->array();

                if(la.size() != ra.size()) return false;
                for(size_t i = 0; i < la.size(); ++i) {
                    frames.emplace_back(&la[i], &ra[i]);
                }
                break;
            }
            case Type::TObject: {
                auto& lo = f.left->object();
                auto& ro = f.right->object();

                if(lo.size() != ro.size()) return false;
                for(auto it : lo) {
                    auto proxy = ro.find(it.first());
                    if(proxy == ro.end()) {
                        return false;
                    }
                    frames.emplace_back(&it.second(), &proxy.second());
                }
                break;
            }
            case Type::TNull:
                break;
            }
        }
        return true;
    }

    template<class V>
    inline RefWrapper<typename BasicAData<V>::Array::container_t> BasicAData<V>::Array::safe_visit(std::ptrdiff_t index) {
        if(index < 0) index = values.size() + index;
        [[unlikely]] panic_if(index >= values.size(), "Array out of bounds!");
        return ref(values, index);
    }

    template<class V>
    inline BasicAData<V>& BasicAData<V>::Array::operator[](std::ptrdiff_t index) {
        if(index < 0) index = values.size() + index;
        [[unlikely]] panic_if(index < 0 || index >= values.size() + conf_array_auto_expand, "Array out of bounds!");
        if(index >= values.size()) values.resize(index + 1, BasicAData<V>(values.get_allocator().resource()));
        return values[index];
    }

    template<class V>
    inline const BasicAData<V>& BasicAData<V>::Array::operator[](std::ptrdiff_t index) const {
        if(index < 0) index = values.size() + index;
        [[unlikely]] panic_if(index < 0 || index >= values.size(), "Array out of bounds!");
        return values[index];
    }

    template<class V>
    template<class T> 
    inline T& BasicAData<V>::set() {
        if constexpr(std::is_same_v<T, std::monostate>) {
            return data.template emplace<std::monostate>();
        } else return data.template emplace<T>(allocator);
    }
    
    template<class V>
    template<typename Other, typename MergeFn>
    BasicAData<V>& BasicAData<V>::__merge_impl(Other&& in, MergeFn&& fn) {
        constexpr bool is_rvalue = !std::is_lvalue_reference_v<Other>;
        using data_type = BasicAData<V>;
        using SourcePtr = std::conditional_t<is_rvalue, data_type*, const data_type*>;
        
        struct Job {
            data_type* destination;
            SourcePtr source;
            Job(data_type* d, SourcePtr s) : destination(d), source(s) {}
        };
        
        struct ObjNext {
            size_t index;
            SourcePtr src;
            ObjNext(size_t i, SourcePtr s) : index(i), src(s) {}
        };

        std::vector<Job> jobs, next_jobs;
        std::vector<ObjNext> object_nexts;
        jobs.emplace_back(this, const_cast<SourcePtr>(&in));

        while(!jobs.empty()) {
            while(!jobs.empty()) {
                auto [dest, src] = jobs.back();
                jobs.pop_back();

                if(dest->is_object() && src->is_object()) {
                    auto& dobj = dest->object();
                    auto& sobj = const_cast<std::remove_const_t<std::remove_reference_t<decltype(src->object())>>&>(src->object());
                    
                    object_nexts.clear();
                    for(auto mit : sobj) {
                        auto it = dobj.find(mit.first());
                        if(it != dobj.end()) {
                            object_nexts.emplace_back(it.it->second, &mit.second());
                        } else {
                            if constexpr(is_rvalue) {
                                dobj[mit.first()] = std::move(mit.second());
                            } else {
                                dobj[mit.first()] = mit.second();
                            }
                        }
                    }
                    for(auto [i, n] : object_nexts) {
                        next_jobs.emplace_back(&dobj.children[i], n);
                    }
                } else if(fn(*dest, *src) == MergeOperation::Override) {
                    if constexpr(is_rvalue) {
                        dest->rewrite(std::move(*const_cast<data_type*>(src)));
                    } else {
                        dest->rewrite(*src);
                    }
                }
            }
            jobs = std::move(next_jobs);
            next_jobs.clear();
        }

        return *this;
    }

    template<class V>
    template<IsDiffFn<BasicAData<V>> DiffFn>
    inline bool BasicAData<V>::diff(
        const BasicAData<V>& in,
        BasicAData<V>* added_or_modified,
        BasicAData<V>* src_lack_of,
        DiffFn&& fn
    ) const {
        struct Job {
            const BasicAData<V>* destination;
            const BasicAData<V>* source;
            BasicAData<V>* current_add;
            BasicAData<V>* current_deleted;
        };
        bool ret = false;
        std::vector<Job> jobs, next_jobs;
        jobs.emplace_back(Job{this, &in, added_or_modified, src_lack_of});

        while(!jobs.empty()) {
            while(!jobs.empty()) {
                auto [dest, src, cadd, cdel] = jobs.back();
                jobs.pop_back();

                if(dest->is_object() && src->is_object()) {
                    auto& dobj = dest->object();
                    auto& sobj = src->object();

                    if(cadd) {
                        cadd->template set<typename BasicAData<V>::Object>();
                        cadd->object().children.reserve(sobj.size());
                    }
                    if(cdel) {
                        cdel->template set<typename BasicAData<V>::Object>();
                        cdel->object().children.reserve(std::max(dobj.size(), sobj.size()));
                    }

                    for(auto mit : dobj) {
                        auto it = sobj.find(mit.first());
                        if(it == sobj.end()) {
                            if(!added_or_modified && !src_lack_of) return true;
                            ret = true;
                            if(src_lack_of) (*cdel)[mit.first()] = mit.second();
                        }
                    }

                    for(auto mit : sobj) {
                        auto it = dobj.find(mit.first());
                        if(it != dobj.end()) {
                            jobs.emplace_back(Job{
                                &mit.second(),
                                &it.second(),
                                cadd ? &(*cadd)[mit.first()] : nullptr,
                                cdel ? &(*cdel)[mit.first()] : nullptr
                            });
                        } else {
                            if(!added_or_modified && !src_lack_of) return true;
                            ret = true;
                            if(added_or_modified) (*cadd)[mit.first()] = mit.second();
                        }
                    }
                    
                } else if(fn(*dest, *src) == MergeOperation::Override) {
                    if(!added_or_modified && !src_lack_of) return true;
                    ret = true;
                    if(added_or_modified) cadd->rewrite(*src);
                }
            }

            jobs = std::move(next_jobs);
            next_jobs.clear();
        }

        return ret;
    }

    template<class V>
    template<bool PruneArray, IsPruneFn<BasicAData<V>> EmptyFn>
    BasicAData<V>& BasicAData<V>::prune(EmptyFn&& fn) {
        struct Frame {
            BasicAData<V>* current;
            bool expanded {false};
        };
        std::deque<Frame> frames;
        std::vector<std::string_view> key_rms;
        frames.emplace_back(Frame{this, false});

        while(!frames.empty()) {
            Frame& d = frames.back();
            if(d.current->is_object()) {
                auto& obj = d.current->object();
                if(obj.size()) {
                    key_rms.clear();
                    size_t pop_p = frames.size() - 1;
                    for(auto mit : d.current->object()) {
                        if(fn(mit.second())) {
                            key_rms.emplace_back(mit.first());
                        } else {
                            if(!d.expanded) frames.emplace_back(Frame{&mit.second(), false});
                        }
                    }
                    for(auto k : key_rms) {
                        obj.remove(k);
                    }
                    if(!d.expanded) {
                        if(fn(*d.current)) {
                            d.current->set_null();
                            frames.erase(frames.begin() + pop_p, frames.end());
                        }
                        d.expanded = true;
                        continue;
                    }
                }
            }
            if constexpr(PruneArray) {
                if(d.current->is_array()) {
                    auto& arr = d.current->array();
                    if(arr.size()) {
                        for(auto it = arr.values.begin(); it < arr.values.end(); ) {
                            if(fn(*it)) {
                                arr.values.erase(it);
                            } else ++it;
                        }
                        if(!d.expanded) {
                            if(fn(*d.current)) {
                                d.current->set_null();
                                frames.pop_back();
                            } else {
                                for(auto& i : arr.values) {
                                    frames.emplace_back(Frame{&i, false});
                                }
                            }
                            d.expanded = true;
                            continue;
                        }
                    }
                }
            }
            if(fn(*d.current)) {
                d.current->set_null();
            }
            frames.pop_back();
        }
        return *this;
    }
} // namespace alib5

#endif
