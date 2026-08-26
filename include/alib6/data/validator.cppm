/**
 * @file validator.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Schema 结构校验器 (alib6.data:validator)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.data:validator;
import std;
import alib6.core;
import :concepts;
import :kernel;

namespace pmr = std::pmr;

export namespace alib6::data {

    /// Schema 约束 Object 的 Magic Key
    inline constexpr std::string_view magic_key_for_schema_restr = "[ALIB6_OBJ]";
    inline constexpr std::string_view magic_key_for_schema_restr_legacy = "[ALIB5_OBJ]";

    /**
     * @brief AData 动态数据结构校验器
     */
    struct Validator {

        /**
         * @brief 校验规则节点
         */
        struct Node {
            enum TypeRestrict : u8 {
                RNone,
                RNull,
                RValue,
                RString,
                RInt,
                RDouble,
                RBool,
                RArray,
                RObject
            };

            bool required{true};
            TypeRestrict type_restrict{RNone};

            Value min_length;
            Value max_length;

            struct Validate {
                std::string_view method;
                pmr::vector<pmr::string> args;

                explicit Validate(memory_resource* mem = get_default_resource())
                    : args(mem) {}

                Validate(const Validate& other, memory_resource* mem)
                    : method(other.method), args(other.args, mem) {}

                Validate(Validate&& other, memory_resource* mem)
                    : method(other.method), args(std::move(other.args), mem) {}

                Validate(const Validate&) = default;
                Validate(Validate&&) noexcept = default;
                Validate& operator=(const Validate&) = default;
                Validate& operator=(Validate&&) noexcept = default;
            };

            pmr::vector<Validate> validates;
            std::optional<AData> default_value{std::nullopt};

            pmr::unordered_set<
                pmr::string,
                alib6::TransparentStringHash,
                alib6::TransparentStringEqual
            > enums;

            pmr::unordered_map<pmr::string, Node> children;
            pmr::vector<Node> array_subs;

            bool is_tuple{false};
            bool override_if_conflict{false};

            explicit Node(memory_resource* mem = get_default_resource())
                : min_length(mem)
                , max_length(mem)
                , validates(mem)
                , enums(mem)
                , children(mem)
                , array_subs(mem) {
                reset();
            }

            Node(const Node& other, memory_resource* mem)
                : required(other.required)
                , type_restrict(other.type_restrict)
                , min_length(other.min_length, mem)
                , max_length(other.max_length, mem)
                , validates(mem)
                , default_value(std::nullopt)
                , enums(other.enums, mem)
                , children(other.children, mem)
                , array_subs(other.array_subs, mem)
                , is_tuple(other.is_tuple)
                , override_if_conflict(other.override_if_conflict) {
                validates.reserve(other.validates.size());
                for (const auto& v : other.validates) {
                    validates.emplace_back(v, mem);
                }
                if (other.default_value) {
                    default_value.emplace(*other.default_value, mem);
                }
            }

            Node(const Node& other)
                : Node(other, other.validates.get_allocator().resource()) {}

            Node(Node&& other) noexcept = default;

            Node(Node&& other, memory_resource* mem)
                : required(other.required)
                , type_restrict(other.type_restrict)
                , min_length(std::move(other.min_length))
                , max_length(std::move(other.max_length))
                , validates(std::move(other.validates), mem)
                , default_value(std::move(other.default_value))
                , enums(std::move(other.enums), mem)
                , children(std::move(other.children), mem)
                , array_subs(std::move(other.array_subs), mem)
                , is_tuple(other.is_tuple)
                , override_if_conflict(other.override_if_conflict) {}

            Node& operator=(Node&&) noexcept = default;

            Node& operator=(const Node& other) {
                if (this == &other) return *this;
                required = other.required;
                type_restrict = other.type_restrict;
                min_length = other.min_length;
                max_length = other.max_length;
                validates = other.validates;
                default_value = other.default_value;
                enums = other.enums;
                children = other.children;
                array_subs = other.array_subs;
                is_tuple = other.is_tuple;
                override_if_conflict = other.override_if_conflict;
                return *this;
            }

            void reset() {
                required = true;
                type_restrict = RNone;
                min_length.set("");
                max_length.set("");
                validates.clear();
                default_value = std::nullopt;
                enums.clear();
                children.clear();
                array_subs.clear();
                is_tuple = false;
                override_if_conflict = false;
            }
        };

        /**
         * @brief 校验结果反馈载体
         */
        struct Result {
            bool enable_string_errors{true};
            bool enable_missing{true};
            bool success{true};
            pmr::vector<pmr::string> recorded_errors;

            pmr::unordered_set<
                pmr::string,
                alib6::TransparentStringHash,
                alib6::TransparentStringEqual
            > missings;

            explicit Result(memory_resource* mem = get_default_resource())
                : recorded_errors(mem), missings(mem) {}

            void reset() {
                success = true;
                recorded_errors.clear();
                missings.clear();
            }

            template<class... Args>
            void record_error(std::format_string<Args...> fmt, Args&&... args) {
                if (enable_string_errors) {
                    auto& str = recorded_errors.emplace_back();
                    std::format_to(std::back_inserter(str), fmt, std::forward<Args>(args)...);
                }
                success = false;
            }

            void missing_validate(std::string_view m) {
                if (enable_missing) missings.emplace(m);
            }
        };

        using ValidateMethod = std::function<bool(AData& node, pmr::vector<pmr::string>& args)>;

        Node root;
        alib6::str::StringPool<pmr::string> activated_validates_str_pool;
        pmr::unordered_map<std::string_view, ValidateMethod> validates;
        memory_resource* allocator;

        explicit Validator(memory_resource* mem = get_default_resource())
            : root(mem), activated_validates_str_pool(mem), validates(mem), allocator(mem) {}

        Validator(const AData& d, memory_resource* mem = get_default_resource())
            : root(mem), activated_validates_str_pool(mem), validates(mem), allocator(mem) {
            from_adata(d);
        }

        ValidateMethod& emplace_validate_method(std::string_view name, ValidateMethod method) {
            return validates.emplace(activated_validates_str_pool.get(name), std::move(method)).first->second;
        }

        /**
         * @brief 从 AData 树解析 Schema 约束规则
         * @return pmr::string 解析中的错误信息 (长度为 0 表示无错误成功)
         */
        pmr::string from_adata(const AData& doc);

        /**
         * @brief 校验目标 AData 数据树
         */
        bool validate(AData& doc, Result& result, bool ignore_missing = false);

        [[nodiscard]] Result validate(AData& doc, bool ignore_missing = false) {
            Result r(allocator);
            validate(doc, r, ignore_missing);
            return r;
        }
    };

    namespace debug {
        /**
         * @brief 调试打印 Validator 规则树结构
         */
        void print_validator(const Validator::Node& node, int depth = 0, const std::string& label = "");
    }

} // namespace alib6::data
