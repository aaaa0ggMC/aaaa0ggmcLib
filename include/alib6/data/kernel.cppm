/**
 * @file kernel.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 现代化动态数据树核心 (alib6.data:kernel)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>
#include <cmath>
#include <charconv>

export module alib6.data:kernel;
import std;
import alib6.core;
import :concepts;

namespace pmr = std::pmr;

export namespace alib6::data {

    /// 数组访问自动扩展缓冲上限
    inline constexpr usize conf_array_auto_expand = 4;

    /// 浮点数比对 epsilon 容差
    inline constexpr double conf_value_compare_epsilon = 1e-12;

    class JSON;
    class TOML;

    /**
     * @brief 现代化动态值包装器 (支持 String, Int64, Double, Bool 与 PMR 内存管理)
     */
    struct Value {
        enum Type : u8 {
            STRING,
            INT,
            FLOATING,
            BOOL
        };

    private:
        mutable pmr::string data;
        mutable bool data_dirt{false};
        Type type{STRING};

        union {
            i64 integer;
            double floating;
            bool boolean;
        };

        void mark() const noexcept { data_dirt = true; }

        void back_sync(std::string_view d) const {
            if (data_dirt) {
                data_dirt = false;
                data = d;
            }
        }

    public:
        void sync_to_string() const {
            if (!data_dirt) return;
            switch (type) {
                case INT:      data = alib6::ext::to_string(integer, data.get_allocator().resource()); break;
                case FLOATING: data = alib6::ext::to_string(floating, data.get_allocator().resource()); break;
                case BOOL:     data = alib6::ext::to_string(boolean, data.get_allocator().resource()); break;
                default: break;
            }
            data_dirt = false;
        }

        explicit Value(memory_resource* mem = get_default_resource())
            : data(mem), data_dirt(false), type(STRING), integer(0) {}

        template<class T>
            requires (!alib6::Cast<std::decay_t<T>, std::string_view> &&
                      !std::same_as<std::decay_t<T>, Value> &&
                      IsNodeValue<T>)
        Value(T&& d, memory_resource* mem = get_default_resource())
            : data(mem), data_dirt(true), type(STRING), integer(0) {
            set(std::forward<T>(d));
        }

        template<alib6::Cast<std::string_view> STR>
            requires (!std::same_as<std::decay_t<STR>, Value>)
        Value(STR&& d, memory_resource* mem = get_default_resource())
            : data(std::string_view(d), mem), data_dirt(false), type(STRING), integer(0) {}

        Value(const Value& other, memory_resource* mem = get_default_resource())
            : data(other.data, mem), data_dirt(other.data_dirt), type(other.type), integer(other.integer) {}

        Value(Value&& other) noexcept
            : data(std::move(other.data)), data_dirt(other.data_dirt), type(other.type), integer(other.integer) {
            other.data_dirt = false;
            other.type = STRING;
            other.data.clear();
        }

        Value(Value&& other, memory_resource* mem)
            : data(std::move(other.data), mem), data_dirt(other.data_dirt), type(other.type), integer(other.integer) {
            if (mem == other.data.get_allocator().resource()) {
                other.data_dirt = true;
                other.data.clear();
            }
        }

        Value& operator=(const Value& other) {
            if (this == &other) return *this;
            data = other.data;
            data_dirt = other.data_dirt;
            type = other.type;
            integer = other.integer;
            return *this;
        }

        Value& operator=(Value&& other) noexcept {
            if (this == &other) return *this;
            data = std::move(other.data);
            data_dirt = other.data_dirt;
            type = other.type;
            integer = other.integer;
            return *this;
        }

        template<IsNodeValue T>
        Value& operator=(T&& val) {
            set(std::forward<T>(val));
            return *this;
        }

        [[nodiscard]] Type get_type() const noexcept { return type; }
        [[nodiscard]] std::string_view raw_view() const noexcept { return data; }
        [[nodiscard]] memory_resource* get_allocator() const noexcept { return data.get_allocator().resource(); }

        template<class T>
        auto& reconstruct() {
            if constexpr (alib6::Cast<T, std::string_view>) {
                type = STRING;
                data_dirt = false;
                return data;
            } else {
                data_dirt = true;
                if constexpr (std::same_as<T, bool>) {
                    type = BOOL;
                    return boolean;
                } else if constexpr (std::integral<T>) {
                    type = INT;
                    return integer;
                } else if constexpr (std::floating_point<T>) {
                    type = FLOATING;
                    return floating;
                } else {
                    static_assert(std::same_as<T, bool>, "Unsupported type for reconstruct!");
                }
            }
        }

        template<class TP, bool invoke_err = true>
        auto& transform(ErrorWrapper err = {}) {
            using T = std::remove_const_t<TP>;
            auto generator = [this, err]<class T1, class T2, class T3, Type t1, Type t2, Type t3>(T1& v1, T2& v2, T3& v3) mutable -> auto& {
                if (type == t1) {
                    // 已是当前类型
                } else if (type == t2) {
                    v1 = static_cast<T1>(v2);
                } else if (type == t3) {
                    v1 = static_cast<T1>(v3);
                } else if (type == STRING) {
                    if constexpr (std::same_as<T1, bool>) {
                        if (data == "true" || data == "1" || data == "TRUE" || data == "True") {
                            v1 = true;
                        } else if (data == "false" || data == "0" || data == "FALSE" || data == "False") {
                            v1 = false;
                        } else {
                            v1 = false;
                            if constexpr (invoke_err) {
                                err.report("Failed to format '{}' to target bool.", data);
                            }
                        }
                    } else {
                        std::from_chars_result result{};
                        v1 = alib6::ext::to_T<T1>(alib6::str::trim(data), &result);
                        if constexpr (invoke_err) {
                            if (result.ec != std::errc()) {
                                err.report("Failed to format '{}' to target number.", data);
                            }
                        }
                    }
                }
                type = t1;
                data_dirt = true;
                return v1;
            };

            if constexpr (alib6::Cast<T, std::string_view>) {
                sync_to_string();
                type = STRING;
                return data;
            } else if constexpr (std::same_as<T, bool>) {
                return generator.template operator()<bool, double, i64, BOOL, FLOATING, INT>(boolean, floating, integer);
            } else if constexpr (std::integral<T>) {
                return generator.template operator()<i64, double, bool, INT, FLOATING, BOOL>(integer, floating, boolean);
            } else if constexpr (std::floating_point<T>) {
                return generator.template operator()<double, i64, bool, FLOATING, INT, BOOL>(floating, integer, boolean);
            } else {
                static_assert(std::same_as<T, bool>, "Unsupported type for transform!");
            }
        }

        template<class T>
        Value& set(T&& val) {
            auto& ref = reconstruct<std::decay_t<T>>();
            if constexpr (alib6::Cast<T, std::string_view>) {
                ref = std::string_view(val);
            } else {
                ref = static_cast<std::decay_t<T>>(val);
            }
            return *this;
        }

        template<class T>
        auto expect() const {
            auto make_value = [this]<class T1>(T1 val) {
                if constexpr (alib6::Cast<T, std::string_view>) {
                    if (data_dirt) back_sync(alib6::ext::to_string(val, data.get_allocator().resource()));
                    return std::make_pair(std::string_view(data), true);
                } else {
                    return std::make_pair(static_cast<T>(val), true);
                }
            };

            switch (type) {
                case STRING: {
                    if constexpr (alib6::Cast<T, std::string_view>) {
                        return std::make_pair(std::string_view(data), true);
                    } else if constexpr (std::same_as<T, bool>) {
                        if (data == "true" || data == "1" || data == "TRUE" || data == "True") return std::make_pair(true, true);
                        if (data == "false" || data == "0" || data == "FALSE" || data == "False") return std::make_pair(false, true);
                        return std::make_pair(false, false);
                    } else {
                        std::from_chars_result result{};
                        T v{};
                        auto r = std::from_chars(data.data(), data.data() + data.size(), v);
                        return std::make_pair(v, r.ec == std::errc() && r.ptr == (data.data() + data.size()));
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
        auto to(ErrorWrapper err = {}) const {
            auto ret = expect<T>();
            if (!ret.second) {
                err.report("Cannot format '{}' correctly to target type.", data);
            }
            return ret.first;
        }

        static bool equals(const Value& left, const Value& right, CompareStrategy st = CompareStrategy::Strict) {
            auto float_equal = [](double a, double b) {
                if (a == b) return true;
                double diff = std::abs(a - b);
                if (diff < conf_value_compare_epsilon) return true;
                return diff <= ((std::abs(a) < std::abs(b) ? std::abs(b) : std::abs(a)) * std::numeric_limits<double>::epsilon());
            };

            auto lt = left.get_type();
            auto rt = right.get_type();

            if (lt == rt) {
                switch (lt) {
                    case Type::BOOL:
                        return left.boolean == right.boolean;
                    case Type::FLOATING:
                        if (st == CompareStrategy::Strict) return left.floating == right.floating;
                        return float_equal(left.floating, right.floating);
                    case Type::INT:
                        return left.integer == right.integer;
                    case Type::STRING:
                        return left.data == right.data;
                }
            }

            if (st == CompareStrategy::Strict) return false;

            auto is_numeric = [](Type t) {
                return t == Type::FLOATING || t == Type::INT || t == Type::BOOL;
            };

            if (is_numeric(lt) && is_numeric(rt)) {
                if (lt == Type::BOOL || rt == Type::BOOL) {
                    if (st == CompareStrategy::BoolStrict) {
                        return float_equal(left.to<double>(), right.to<double>());
                    }
                    return left.to<bool>() == right.to<bool>();
                }
                return float_equal(left.to<double>(), right.to<double>());
            }

            if (st == CompareStrategy::BoolStrict || st == CompareStrategy::Lesser) return false;

            const auto& plt = (lt == Type::STRING) ? right : left;
            const auto& prt = (lt == Type::STRING) ? left : right;
            lt = plt.get_type();

            if (lt == Type::FLOATING) {
                auto db = prt.expect<double>();
                if (!db.second) return false;
                return float_equal(db.first, plt.floating);
            } else if (lt == Type::INT) {
                auto db = prt.expect<double>();
                if (!db.second) return false;
                return float_equal(db.first, static_cast<double>(plt.integer));
            } else if (lt == Type::BOOL) {
                std::string_view s = prt.data;
                if (plt.boolean) {
                    return s == "true" || s == "TRUE" || s == "1" || s == "yes" || s == "YES";
                }
                return s == "false" || s == "FALSE" || s == "0" || s == "no" || s == "NO";
            }

            return false;
        }

        [[nodiscard]] bool equals(const Value& b, CompareStrategy st = CompareStrategy::Strict) const {
            return equals(*this, b, st);
        }

        template<class Target>
        void write_to_log(Target& tg) const {
            sync_to_string();
            tg.append(data);
        }
    };

    using CacheValue = Value;

    /**
     * @brief 现代化动态数据节点树 (代表 Null, Value, Object 或 Array)
     */
    template<class ValueType = Value>
    struct BasicAData {
        using value_type = ValueType;
        using data_type = BasicAData<value_type>;

        /**
         * @brief 字典/对象表示 (Key -> 子节点索引映射与 FreelistLinearStorage 槽位管理)
         */
        struct Object {
            using container_t = alib6::storage::FreelistLinearStorage<data_type>;

            container_t children;
            pmr::unordered_map<
                pmr::string,
                usize,
                alib6::TransparentStringHash,
                alib6::TransparentStringEqual
            > object_mapper;

            explicit Object(memory_resource* mem = get_default_resource())
                : children(0, mem), object_mapper(mem) {}

            Object(const Object& other, memory_resource* mem = get_default_resource())
                : children(other.children, mem), object_mapper(other.object_mapper, mem) {}

            Object(Object&& other) noexcept = default;

            Object(Object&& other, memory_resource* mem)
                : children(std::move(other.children), mem), object_mapper(std::move(other.object_mapper), mem) {}

            Object& operator=(const Object& other) = default;
            Object& operator=(Object&& other) noexcept = default;

            void reserve(usize buffer_size) {
                children.reserve(buffer_size);
            }

            std::pair<data_type*, usize> ensure_node(std::string_view key) {
                auto it = object_mapper.find(key);
                if (it != object_mapper.end()) {
                    return { &children[it->second], it->second };
                }

                auto alloc = object_mapper.get_allocator();
                usize index = 0;
                bool is_new = false;
                data_type& node = children.try_next_with_index(is_new, index, alloc.resource());
                object_mapper.emplace(pmr::string(key, alloc), index);
                return { &node, index };
            }

            bool rename(std::string_view old_name, std::string_view new_name) {
                auto it = object_mapper.find(old_name);
                if (it == object_mapper.end()) return false;
                if (old_name == new_name) return true;
                usize index = it->second;
                object_mapper.erase(it);
                object_mapper.emplace(pmr::string(new_name, object_mapper.get_allocator()), index);
                return true;
            }

            bool remove(std::string_view name) {
                auto it = object_mapper.find(name);
                if (it == object_mapper.end()) return false;
                children.remove(it->second);
                object_mapper.erase(it);
                return true;
            }

            [[nodiscard]] auto safe_visit(std::string_view visit) {
                return alib6::ref(children.data, ensure_node(visit).second);
            }

            [[nodiscard]] data_type& operator[](std::string_view visit) {
                return *ensure_node(visit).first;
            }

            [[nodiscard]] const data_type* at_ptr(std::string_view visit) const {
                auto it = object_mapper.find(visit);
                if (it == object_mapper.end()) return nullptr;
                return &children.data[it->second];
            }

            [[nodiscard]] data_type* at_ptr(std::string_view visit) {
                return const_cast<data_type*>(
                    static_cast<const Object*>(this)->at_ptr(visit)
                );
            }

            void clear() {
                children.clear();
                object_mapper.clear();
            }

            [[nodiscard]] const data_type& operator[](std::string_view visit) const {
                auto a = at_ptr(visit);
                panicf_if(!a, "Invalid visit '{}' in AData::Object!", visit);
                return *a;
            }

            [[nodiscard]] usize size() const noexcept { return object_mapper.size(); }
            [[nodiscard]] bool empty() const noexcept { return object_mapper.empty(); }
            [[nodiscard]] bool contains(std::string_view key) const noexcept {
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
                using cont_ptr = std::conditional_t<is_const, const container_t*, container_t*>;

                it_type it;
                cont_ptr cont{nullptr};

                struct Proxy {
                    const pmr::string& _first;
                    reference _second;

                    [[nodiscard]] auto& first() const noexcept { return _first; }
                    [[nodiscard]] reference second() const noexcept { return _second; }
                };

                ObjectIterator() = default;
                ObjectIterator(it_type iter, cont_ptr container) : it(iter), cont(container) {}

                [[nodiscard]] auto& first() const noexcept { return it->first; }
                [[nodiscard]] auto& second() const noexcept { return cont->data[it->second]; }

                Proxy operator*() const noexcept { return Proxy{it->first, cont->data[it->second]}; }
                ObjectIterator& operator++() noexcept { ++it; return *this; }
                ObjectIterator operator++(int) noexcept { auto old = *this; ++it; return old; }
                bool operator==(const ObjectIterator& other) const noexcept { return it == other.it; }
                bool operator!=(const ObjectIterator& other) const noexcept { return it != other.it; }
                Proxy operator->() const noexcept { return Proxy{it->first, cont->data[it->second]}; }
            };

            using iterator = ObjectIterator<false>;
            using const_iterator = ObjectIterator<true>;

            iterator begin() { return {object_mapper.begin(), &children}; }
            const_iterator begin() const { return {object_mapper.begin(), &children}; }
            iterator end() { return {object_mapper.end(), &children}; }
            const_iterator end() const { return {object_mapper.end(), &children}; }

            iterator find(std::string_view d) { return {object_mapper.find(d), &children}; }
            const_iterator find(std::string_view d) const { return {object_mapper.find(d), &children}; }
        };

        /**
         * @brief 数组表示 (有序 BasicAData 集合)
         */
        struct Array {
            using container_t = pmr::vector<data_type>;
            container_t values;

            explicit Array(memory_resource* mem = get_default_resource())
                : values(mem) {}

            Array(const Array& other, memory_resource* mem = get_default_resource())
                : values(other.values, mem) {}

            Array(Array&& other) noexcept = default;

            Array(Array&& other, memory_resource* mem)
                : values(std::move(other.values), mem) {}

            Array& operator=(const Array& other) = default;
            Array& operator=(Array&& other) noexcept = default;

            [[nodiscard]] auto safe_visit(ptrdiff_t index) {
                if (index < 0) index = static_cast<ptrdiff_t>(values.size()) + index;
                panic_if(index < 0 || static_cast<usize>(index) >= values.size(), "Array out of bounds in safe_visit!");
                return alib6::ref(values, static_cast<usize>(index));
            }

            [[nodiscard]] data_type& operator[](ptrdiff_t index) {
                if (index < 0) index = static_cast<ptrdiff_t>(values.size()) + index;
                panic_if(index < 0 || static_cast<usize>(index) >= values.size() + conf_array_auto_expand, "Array index out of bounds!");
                if (static_cast<usize>(index) >= values.size()) {
                    values.resize(static_cast<usize>(index) + 1, data_type(values.get_allocator().resource()));
                }
                return values[static_cast<usize>(index)];
            }

            [[nodiscard]] const data_type& operator[](ptrdiff_t index) const {
                if (index < 0) index = static_cast<ptrdiff_t>(values.size()) + index;
                panic_if(index < 0 || static_cast<usize>(index) >= values.size(), "Array index out of bounds!");
                return values[static_cast<usize>(index)];
            }

            void reserve(usize size) { values.reserve(size); }

            std::span<data_type> ensure(usize size) {
                if (size <= values.size()) return {};
                auto res = values.get_allocator().resource();
                auto beg = values.size();
                values.resize(size, data_type(res));
                return std::span(values.begin() + beg, values.end());
            }

            [[nodiscard]] usize size() const noexcept { return values.size(); }
            void clear() noexcept { values.clear(); }
            [[nodiscard]] bool empty() const noexcept { return values.empty(); }

            [[nodiscard]] const data_type* at_ptr(ptrdiff_t index) const noexcept {
                if (index < 0) index = static_cast<ptrdiff_t>(values.size()) + index;
                if (index < 0 || static_cast<usize>(index) >= values.size()) return nullptr;
                return &values[static_cast<usize>(index)];
            }

            [[nodiscard]] data_type* at_ptr(ptrdiff_t index) noexcept {
                return const_cast<data_type*>(
                    static_cast<const Array*>(this)->at_ptr(index)
                );
            }

            auto begin() noexcept { return values.begin(); }
            auto begin() const noexcept { return values.begin(); }
            auto end() noexcept { return values.end(); }
            auto end() const noexcept { return values.end(); }
        };

        enum Type : u8 {
            TNull,
            TValue,
            TObject,
            TArray
        };

    private:
        std::variant<std::monostate, value_type, Object, Array> data;
        memory_resource* allocator;

        template<class T>
        auto& __get_value() const {
            using type = std::decay_t<T>;
            const type* f = std::get_if<type>(&data);
            if (f) return *f;
            panicf("AData type mismatch! CurrentId:{} (0 null, 1 value, 2 object, 3 array)", static_cast<int>(get_type()));
        }

        template<class T>
        auto& __get_value() {
            return const_cast<std::decay_t<T>&>(
                static_cast<const BasicAData*>(this)->__get_value<T>()
            );
        }

        template<class T>
        auto& __ensure_type() {
            if (std::holds_alternative<std::monostate>(data)) {
                return set<T>();
            }
            return __get_value<T>();
        }

        static auto clone_data(const decltype(data)& src, memory_resource* res) {
            return std::visit([res](auto&& v) -> decltype(data) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::same_as<T, std::monostate>) return v;
                else if constexpr (std::same_as<T, value_type>) {
                    value_type x(res);
                    x = v;
                    return x;
                } else if constexpr (std::same_as<T, Object>) {
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
        [[nodiscard]] memory_resource* get_allocator() const noexcept { return allocator; }

        template<class T>
        T& set() {
            if constexpr (std::same_as<T, std::monostate>) {
                return data.template emplace<std::monostate>();
            } else {
                return data.template emplace<T>(allocator);
            }
        }

        auto& set_null() { return set<std::monostate>(); }
        auto& _set_object() { return set<Object>(); }
        auto& _set_array() { return set<Array>(); }
        auto& _set_value() { return set<value_type>(); }

        explicit BasicAData(memory_resource* mem = get_default_resource())
            : allocator(mem) {}

        template<IsNodeValue T>
            requires (!std::same_as<std::remove_cvref_t<T>, BasicAData>)
        BasicAData(T&& val, memory_resource* mem = get_default_resource())
            : allocator(mem) {
            this->operator=(std::forward<T>(val));
        }

        BasicAData(const BasicAData& other)
            : allocator(other.allocator), data(clone_data(other.data, other.allocator)) {}

        BasicAData(BasicAData&& other) noexcept
            : allocator(other.allocator), data(std::move(other.data)) {}

        BasicAData(const BasicAData& other, memory_resource* mem)
            : allocator(mem), data(clone_data(other.data, mem)) {}

        BasicAData(const Object& other, memory_resource* mem = get_default_resource())
            : allocator(mem) {
            set<Object>() = other;
        }

        BasicAData(const Array& other, memory_resource* mem = get_default_resource())
            : allocator(mem) {
            set<Array>() = other;
        }

        BasicAData(const value_type& other, memory_resource* mem = get_default_resource())
            : allocator(mem) {
            set<value_type>() = other;
        }

        BasicAData(const std::monostate&, memory_resource* mem = get_default_resource())
            : allocator(mem) {
            set<std::monostate>();
        }

        BasicAData(std::initializer_list<const BasicAData> list, memory_resource* mem = get_default_resource())
            : allocator(mem) {
            *this = list;
        }

        ~BasicAData() = default;

        [[nodiscard]] BasicAData detach(memory_resource* mem = get_default_resource()) const {
            return BasicAData(*this, mem);
        }

        [[nodiscard]] Type get_type() const noexcept { return static_cast<Type>(data.index()); }
        [[nodiscard]] bool is_null() const noexcept { return get_type() == TNull; }
        [[nodiscard]] bool is_object() const noexcept { return get_type() == TObject; }
        [[nodiscard]] bool is_array() const noexcept { return get_type() == TArray; }
        [[nodiscard]] bool is_value() const noexcept { return get_type() == TValue; }

        [[nodiscard]] value_type& value() { return __get_value<value_type>(); }
        [[nodiscard]] Object& object() { return __get_value<Object>(); }
        [[nodiscard]] Array& array() { return __get_value<Array>(); }
        [[nodiscard]] std::monostate& null() { return __get_value<std::monostate>(); }

        [[nodiscard]] const value_type& value() const { return __get_value<value_type>(); }
        [[nodiscard]] const Object& object() const { return __get_value<Object>(); }
        [[nodiscard]] const Array& array() const { return __get_value<Array>(); }
        [[nodiscard]] const std::monostate& null() const { return __get_value<std::monostate>(); }

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

        BasicAData& rewrite(BasicAData&& other) {
            if (this == &other) return *this;
            auto d = std::move(other.data);
            other.set<std::monostate>();

            if (this->allocator == other.allocator) {
                this->data = std::move(d);
            } else {
                this->data = clone_data(d, allocator);
            }
            return *this;
        }

        BasicAData& operator=(std::initializer_list<const BasicAData> list) {
            if (!is_array() && !is_null()) {
                panic("Implicit type cast to Array is forbidden.");
            }
            auto& arr = this->set<Array>();
            arr.values.reserve(list.size());
            for (auto&& item : list) {
                arr.values.emplace_back(item, this->allocator);
            }
            return *this;
        }

        BasicAData& operator=(BasicAData&& val) noexcept {
            return rewrite(std::move(val));
        }

        BasicAData& operator=(const BasicAData& val) {
            if (this == &val) return *this;
            auto cloned = clone_data(val.data, allocator);
            this->data = std::move(cloned);
            return *this;
        }

        [[nodiscard]] BasicAData& operator[](std::string_view visit) {
            Object& o = __ensure_type<Object>();
            return o[visit];
        }

        [[nodiscard]] const BasicAData& operator[](std::string_view visit) const {
            return object()[visit];
        }

        [[nodiscard]] BasicAData& operator[](ptrdiff_t visit) {
            Array& o = __ensure_type<Array>();
            return o[visit];
        }

        [[nodiscard]] const BasicAData& operator[](ptrdiff_t visit) const {
            return array()[visit];
        }

        template<class T>
        auto to(ErrorWrapper err = {}) const {
            return value().template to<T>(err);
        }

        template<class T>
        auto try_to() const -> std::optional<decltype(std::declval<const value_type>().template to<T>())> {
            if (get_type() != TValue) return std::nullopt;
            auto ret = expect<T>();
            if (!ret.second) return std::nullopt;
            return ret.first;
        }

        template<class T>
        auto expect() const {
            return value().template expect<T>();
        }

        template<class T>
        auto try_expect() const -> std::optional<decltype(std::declval<const value_type>().template expect<T>())> {
            if (get_type() != TValue) return std::nullopt;
            return std::optional(expect<T>());
        }

        /**
         * @brief 根据 JSON Pointer (RFC 6901) 寻址节点指针 (未找到返回 nullptr)
         */
        [[nodiscard]] const BasicAData* jump_ptr(std::string_view path, bool invoke_err = true, ErrorWrapper err = {}) const {
            if (path.empty()) return this;
            if (path[0] != '/') {
                if (invoke_err) err.report("Failed to parse pointer which does not start with '/': {}", path);
                return nullptr;
            }

            auto splits = alib6::str::split(path, '/');
            std::span<std::string_view> main_span = splits;
            main_span = main_span.subspan(1);

            const BasicAData* current = this;
            usize index = 0;
            value_type val(allocator);
            pmr::string unescaped_key(allocator);

            while (index < main_span.size()) {
                std::string_view s = main_span[index];
                ++index;
                val = s;

                if (auto ss = val.template expect<i64>(); ss.second && current->is_array()) {
                    auto* ptr = current->array().at_ptr(static_cast<ptrdiff_t>(ss.first));
                    if (ptr) {
                        current = ptr;
                    } else {
                        if (invoke_err) err.report("Locate failed when finding array index {} in path {}", ss.first, path);
                        return nullptr;
                    }
                } else if (current->is_object()) {
                    // 解码 RFC 6901 转义 (~0 -> ~, ~1 -> /)
                    std::string_view key_view = s;
                    if (s.find('~') != std::string_view::npos) {
                        unescaped_key.clear();
                        for (usize i = 0; i < s.size(); ++i) {
                            if (s[i] == '~' && i + 1 < s.size()) {
                                if (s[i + 1] == '0') {
                                    unescaped_key.push_back('~');
                                    ++i;
                                    continue;
                                } else if (s[i + 1] == '1') {
                                    unescaped_key.push_back('/');
                                    ++i;
                                    continue;
                                }
                            }
                            unescaped_key.push_back(s[i]);
                        }
                        key_view = unescaped_key;
                    }

                    auto* ptr = current->object().at_ptr(key_view);
                    if (ptr) {
                        current = ptr;
                    } else {
                        if (invoke_err) err.report("Locate failed when finding object key '{}' in path {}", key_view, path);
                        return nullptr;
                    }
                } else {
                    if (invoke_err) err.report("Locate failed for lack of depth in path {}", path);
                    return nullptr;
                }
            }
            return current;
        }

        [[nodiscard]] BasicAData* jump_ptr(std::string_view path, bool invoke_err = true, ErrorWrapper err = {}) {
            return const_cast<BasicAData*>(
                static_cast<const BasicAData*>(this)->jump_ptr(path, invoke_err, err)
            );
        }

        [[nodiscard]] BasicAData& jump(std::string_view path, bool invoke_err = true, ErrorWrapper err = {}) {
            BasicAData* ptr = jump_ptr(path, invoke_err, err);
            panicf_if(!ptr, "Failed to locate object with path '{}'!", path);
            return *ptr;
        }

        [[nodiscard]] const BasicAData& jump(std::string_view path, bool invoke_err = true, ErrorWrapper err = {}) const {
            const BasicAData* ptr = jump_ptr(path, invoke_err, err);
            panicf_if(!ptr, "Failed to locate object with path '{}'!", path);
            return *ptr;
        }

        static bool equals(const BasicAData& left, const BasicAData& right, CompareStrategy st = CompareStrategy::Strict) {
            struct Frame {
                const BasicAData* left;
                const BasicAData* right;
            };
            pmr::vector<Frame> frames(left.get_allocator());
            frames.push_back({&left, &right});

            while (!frames.empty()) {
                Frame f = frames.back();
                frames.pop_back();

                auto lt = f.left->get_type();
                auto rt = f.right->get_type();
                if (lt != rt) return false;

                switch (lt) {
                    case Type::TValue:
                        if (!f.left->value().equals(f.right->value(), st)) return false;
                        break;
                    case Type::TArray: {
                        const auto& la = f.left->array();
                        const auto& ra = f.right->array();
                        if (la.size() != ra.size()) return false;
                        for (usize i = 0; i < la.size(); ++i) {
                            frames.push_back({&la[static_cast<ptrdiff_t>(i)], &ra[static_cast<ptrdiff_t>(i)]});
                        }
                        break;
                    }
                    case Type::TObject: {
                        const auto& lo = f.left->object();
                        const auto& ro = f.right->object();
                        if (lo.size() != ro.size()) return false;
                        for (auto it : lo) {
                            auto proxy = ro.find(it.first());
                            if (proxy == ro.end()) return false;
                            frames.push_back({&it.second(), &proxy.second()});
                        }
                        break;
                    }
                    case Type::TNull:
                        break;
                }
            }
            return true;
        }

        [[nodiscard]] bool equals(const BasicAData& b, CompareStrategy st = CompareStrategy::Strict) const {
            return equals(*this, b, st);
        }

        static MergeOperation default_merge_fn(BasicAData&, const BasicAData&) noexcept {
            return MergeOperation::Override;
        }

        static MergeOperation default_diff_fn(const BasicAData& dest, const BasicAData& src) noexcept {
            if (dest.equals(src)) return MergeOperation::Skip;
            return MergeOperation::Override;
        }

        static bool default_prune_fn(const BasicAData& d) noexcept {
            if (d.is_null()) return true;
            if (d.is_array() && d.array().empty()) return true;
            if (d.is_object() && d.object().empty()) return true;
            return false;
        }

        template<class Other, class MergeFn>
        BasicAData& merge_impl(Other&& in, MergeFn&& fn) {
            constexpr bool is_rvalue = !std::is_lvalue_reference_v<Other>;
            using SourcePtr = std::conditional_t<is_rvalue, BasicAData*, const BasicAData*>;

            struct Job {
                BasicAData* destination;
                SourcePtr source;
            };

            struct ObjNext {
                usize index;
                SourcePtr src;
            };

            pmr::vector<Job> jobs(get_allocator()), next_jobs(get_allocator());
            pmr::vector<ObjNext> object_nexts(get_allocator());
            jobs.push_back({this, const_cast<SourcePtr>(&in)});

            while (!jobs.empty()) {
                while (!jobs.empty()) {
                    auto [dest, src] = jobs.back();
                    jobs.pop_back();

                    if (dest->is_object() && src->is_object()) {
                        auto& dobj = dest->object();
                        auto& sobj = const_cast<std::remove_const_t<std::remove_reference_t<decltype(src->object())>>&>(src->object());

                        object_nexts.clear();
                        for (auto mit : sobj) {
                            auto it = dobj.find(mit.first());
                            if (it != dobj.end()) {
                                object_nexts.push_back({it.it->second, &mit.second()});
                            } else {
                                if constexpr (is_rvalue) {
                                    dobj[mit.first()] = std::move(mit.second());
                                } else {
                                    dobj[mit.first()] = mit.second();
                                }
                            }
                        }
                        for (auto [i, n] : object_nexts) {
                            next_jobs.push_back({&dobj.children[i], n});
                        }
                    } else if (fn(*dest, *src) == MergeOperation::Override) {
                        if constexpr (is_rvalue) {
                            dest->rewrite(std::move(*const_cast<BasicAData*>(src)));
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

        template<IsMergeFn<BasicAData> MergeFn = decltype(default_merge_fn)>
        BasicAData& merge(const BasicAData& in, MergeFn&& fn = default_merge_fn) {
            return merge_impl<const BasicAData&>(in, std::forward<MergeFn>(fn));
        }

        template<IsMergeFn<BasicAData> MergeFn = decltype(default_merge_fn)>
        BasicAData& merge(BasicAData&& in, MergeFn&& fn = default_merge_fn) {
            return merge_impl<BasicAData&&>(std::move(in), std::forward<MergeFn>(fn));
        }

        template<IsDiffFn<BasicAData> DiffFn = decltype(default_diff_fn)>
        bool diff(
            const BasicAData& in,
            BasicAData* added_or_modified = nullptr,
            BasicAData* src_lack_of = nullptr,
            DiffFn&& fn = default_diff_fn
        ) const {
            struct Job {
                const BasicAData* destination;
                const BasicAData* source;
                BasicAData* current_add;
                BasicAData* current_deleted;
            };

            bool ret = false;
            pmr::vector<Job> jobs(get_allocator()), next_jobs(get_allocator());
            jobs.push_back(Job{this, &in, added_or_modified, src_lack_of});

            while (!jobs.empty()) {
                while (!jobs.empty()) {
                    auto [dest, src, cadd, cdel] = jobs.back();
                    jobs.pop_back();

                    if (dest->is_object() && src->is_object()) {
                        const auto& dobj = dest->object();
                        const auto& sobj = src->object();

                        if (cadd) {
                            cadd->template set<Object>();
                            cadd->object().children.reserve(sobj.size());
                        }
                        if (cdel) {
                            cdel->template set<Object>();
                            cdel->object().children.reserve(std::max(dobj.size(), sobj.size()));
                        }

                        for (auto mit : dobj) {
                            auto it = sobj.find(mit.first());
                            if (it == sobj.end()) {
                                if (!added_or_modified && !src_lack_of) return true;
                                ret = true;
                                if (src_lack_of) (*cdel)[mit.first()] = mit.second();
                            }
                        }

                        for (auto mit : sobj) {
                            auto it = dobj.find(mit.first());
                            if (it != dobj.end()) {
                                next_jobs.push_back(Job{
                                    &it.second(),
                                    &mit.second(),
                                    cadd ? &(*cadd)[mit.first()] : nullptr,
                                    cdel ? &(*cdel)[mit.first()] : nullptr
                                });
                            } else {
                                if (!added_or_modified && !src_lack_of) return true;
                                ret = true;
                                if (added_or_modified) (*cadd)[mit.first()] = mit.second();
                            }
                        }
                    } else if (fn(*dest, *src) == MergeOperation::Override) {
                        if (!added_or_modified && !src_lack_of) return true;
                        ret = true;
                        if (added_or_modified) cadd->rewrite(*src);
                    }
                }
                jobs = std::move(next_jobs);
                next_jobs.clear();
            }
            return ret;
        }

        template<bool PruneArray = false, IsPruneFn<BasicAData> EmptyFn = decltype(default_prune_fn)>
        BasicAData& prune(EmptyFn&& fn = default_prune_fn) {
            struct Frame {
                BasicAData* current;
                bool expanded{false};
            };
            pmr::deque<Frame> frames(get_allocator());
            pmr::vector<std::string_view> key_rms(get_allocator());
            frames.emplace_back(Frame{this, false});

            while (!frames.empty()) {
                Frame& d = frames.back();
                if (d.current->is_object()) {
                    auto& obj = d.current->object();
                    if (!obj.empty()) {
                        key_rms.clear();
                        usize pop_p = frames.size() - 1;
                        for (auto mit : obj) {
                            if (fn(mit.second())) {
                                key_rms.emplace_back(mit.first());
                            } else {
                                if (!d.expanded) frames.emplace_back(Frame{&mit.second(), false});
                            }
                        }
                        for (auto k : key_rms) {
                            obj.remove(k);
                        }
                        if (!d.expanded) {
                            if (fn(*d.current)) {
                                d.current->set_null();
                                frames.erase(frames.begin() + pop_p, frames.end());
                            }
                            d.expanded = true;
                            continue;
                        }
                    }
                }

                if constexpr (PruneArray) {
                    if (d.current->is_array()) {
                        auto& arr = d.current->array();
                        if (!arr.empty()) {
                            for (auto it = arr.values.begin(); it != arr.values.end();) {
                                if (fn(*it)) {
                                    it = arr.values.erase(it);
                                } else {
                                    ++it;
                                }
                            }
                            if (!d.expanded) {
                                if (fn(*d.current)) {
                                    d.current->set_null();
                                    frames.pop_back();
                                } else {
                                    for (auto& i : arr.values) {
                                        frames.emplace_back(Frame{&i, false});
                                    }
                                }
                                d.expanded = true;
                                continue;
                            }
                        }
                    }
                }
                if (fn(*d.current)) {
                    d.current->set_null();
                }
                frames.pop_back();
            }
            return *this;
        }

        template<class DataPolicy = JSON>
            requires IsDataPolicy<DataPolicy, BasicAData>
        auto load_from_memory(std::string_view mem, DataPolicy&& parser = DataPolicy()) {
            return std::forward<DataPolicy>(parser).parse(mem, *this);
        }

        template<class DataPolicy = JSON>
            requires IsDataPolicy<DataPolicy, BasicAData>
        auto load_from_file(std::string_view path, DataPolicy&& parser = DataPolicy(), ErrorWrapper err = {}) {
            auto content = alib6::io::read_all(path, 0, allocator, err);
            return load_from_memory(content, std::forward<DataPolicy>(parser));
        }

        template<class Dumper = JSON, class T>
            requires IsDataPolicy<Dumper, BasicAData>
        auto dump(T& target, Dumper&& dumper = Dumper()) const {
            return std::forward<Dumper>(dumper).dump(target, *this);
        }

        template<class Dumper = JSON>
            requires IsDataPolicy<Dumper, BasicAData>
        [[nodiscard]] pmr::string dump_to_string(Dumper&& dumper = Dumper(), memory_resource* mem = nullptr) const {
            if (!mem) mem = allocator;
            pmr::string str(mem);
            dump(str, std::forward<Dumper>(dumper));
            return str;
        }

        template<class Dumper = JSON>
            requires IsDataPolicy<Dumper, BasicAData>
        void dump_to_file(std::string_view file_path, Dumper&& dumper = Dumper(), memory_resource* mem = nullptr, ErrorWrapper err = {}) const {
            auto s = dump_to_string(std::forward<Dumper>(dumper), mem);
            alib6::io::write_all(file_path, s, err);
        }

        template<class Dumper = JSON>
            requires IsDataPolicy<Dumper, BasicAData>
        [[nodiscard]] pmr::string str(Dumper&& dmp = Dumper()) const {
            return dump_to_string(std::forward<Dumper>(dmp));
        }

        template<class STR, class Dumper = JSON>
            requires IsDataPolicy<Dumper, BasicAData>
        void write_to_log(STR& s) const {
            dump(s, Dumper());
        }
    };

    using AData = BasicAData<>;

    template<class Dumper, class V = Value>
    inline BasicAData<V> adata_from_memory(std::string_view mem, memory_resource* mem_res = get_default_resource()) {
        BasicAData<V> data(mem_res);
        data.template load_from_memory<Dumper>(mem);
        return data;
    }

    /**
     * @brief 异构或同构 AData 数据树迁移
     */
    template<class V1, class V2>
    inline void migrate(BasicAData<V2>& dest, const BasicAData<V1>& src) {
        if constexpr (std::same_as<V1, V2>) {
            dest.merge(src, [](auto&, const auto&) {
                return MergeOperation::Override;
            });
            return;
        }

        struct StackItem {
            BasicAData<V2>* d_ptr;
            const BasicAData<V1>* s_ptr;
        };
        pmr::vector<StackItem> stack(dest.get_allocator());
        stack.push_back({&dest, &src});

        while (!stack.empty()) {
            StackItem curr = stack.back();
            stack.pop_back();

            const auto& s = *curr.s_ptr;
            auto& d = *curr.d_ptr;

            if (s.is_null()) {
                d.set_null();
                continue;
            }

            if (s.is_object()) {
                if (!d.is_object()) d._set_object();
                auto& dobj = d.object();
                const auto& sobj = s.object();
                dobj.children.reserve(sobj.size() + dobj.size());

                for (auto it = sobj.begin(); it != sobj.end(); ++it) {
                    stack.push_back({&dobj[it.first()], &it.second()});
                }
            } else if (s.is_array()) {
                if (!d.is_array()) d._set_array();
                auto& darr = d.array();
                const auto& sarr = s.array();
                darr.reserve(sarr.size());
                for (usize i = 0; i < sarr.size(); ++i) {
                    stack.push_back({&darr[static_cast<ptrdiff_t>(i)], &sarr[static_cast<ptrdiff_t>(i)]});
                }
            } else if (s.is_value()) {
                const auto& value = s.value();
                switch (value.get_type()) {
                    case Value::STRING:
                        d.rewrite(value.template to<std::string_view>());
                        break;
                    case Value::FLOATING:
                        d.rewrite(value.template to<double>());
                        break;
                    case Value::INT:
                        d.rewrite(value.template to<i64>());
                        break;
                    case Value::BOOL:
                        d.rewrite(value.template to<bool>());
                        break;
                    default:
                        d.set_null();
                        break;
                }
            }
        }
    }

    template<class V1, class V2>
    inline void migrate(BasicAData<V2>& dest, BasicAData<V1>&& src) {
        if constexpr (std::same_as<V1, V2>) {
            dest.merge(std::move(src), [](auto&, const auto&) {
                return MergeOperation::Override;
            });
        } else {
            migrate(dest, static_cast<const BasicAData<V1>&>(src));
            src.set_null();
        }
    }

    template<class V2 = Value, class V1>
    inline BasicAData<V2> migrate(const BasicAData<V1>& src, memory_resource* mem = get_default_resource()) {
        BasicAData<V2> data(mem);
        migrate(data, src);
        return data;
    }

    template<class V2 = Value, class V1>
    inline BasicAData<V2> migrate(BasicAData<V1>&& src, memory_resource* mem = get_default_resource()) {
        BasicAData<V2> data(mem);
        migrate(data, std::move(src));
        return data;
    }

} // namespace alib6::data
