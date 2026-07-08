/**
 * @file adata_validator.h
 * @brief Schema Validator for ALib5 AData / ALib5 AData 的 Schema 校验器
 *
 * @details
 * English: This file provides the `Validator` structure used to validate `AData` nodes against 
 * predefined schemas. It supports type restrictions, length constraints, enumerations, 
 * default value injections, and custom validation functions.
 * 
 * Chinese: 本文件提供了 `Validator` 结构，用于根据预定义的 schema 对 `AData` 节点进行校验。
 * 它支持类型限制、长度约束、枚举、默认值注入以及自定义校验函数。
 */

#ifndef ALIB5_ADATA_VALIDATOR
#define ALIB5_ADATA_VALIDATOR

#include <alib5/data/kernel.h>
#include <alib5/aparser.h>
#include <format>
#include <optional>
#include <functional>

namespace alib5 {

    /**
     * @brief Magic key used to constrain an object in the schema.
     * 
     * @par Original Comments:
     * English: Magic key to constrain the object in the schema.
     * Chinese: 在schema里面约束object的magic key
     */
    constexpr std::string_view magic_key_for_schema_restr = "[ALIB5_OBJ]";

    /**
     * @brief AData structure validator.
     * 
     * @par Original Comments:
     * English: Validator.
     * Chinese: 校验器
     */
    struct ALIB5_API Validator {
        
        /**
         * @brief Represents a validation rule node within the schema tree.
         */
        struct ALIB5_API Node {
            /**
             * @brief Defines strict type constraints for the current node.
             * 
             * @par Original Comments:
             * English: RValue actually has higher priority than string/int/double/bool.
             * Chinese: 其实优先级大于string int double bool
             */
            enum TypeRestrict {
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

            /**
             * @brief Indicates if the node is required.
             * 
             * @par Original Comments:
             * English: Whether it is required. If required, it will either report an error or construct with a default value. If not required, default value filling will not be performed.
             * Chinese: 是否必要,如果必要,则要么报错要么默认值构造,如果不必要,那么不会进行默认值填充
             */
            bool required;
            
            TypeRestrict type_restrict;
            
            /**
             * @brief Minimum boundary limit.
             * 
             * @par Original Comments:
             * English: For ARRAY, limits the length; for Object, limits the number of members. <0 means no requirement.
             * Chinese: 对于ARRAY,限制长度,对于Object,限制成员数量. <0表示没有要求
             */
            dvalue_t min_length;
            
            /**
             * @brief Maximum boundary limit.
             */
            dvalue_t max_length;

            /**
             * @brief Represents a custom validation method call.
             * 
             * @par Original Comments:
             * English: Validators, arranged in order. Validators can actually modify nodes. string_view points to activated_pool.
             * Chinese: 校验器,依次排列,其实校验器也能修改节点,string_view指向activated_pool
             */
            struct Validate {
                std::string_view method;
                std::pmr::vector<std::pmr::string> args;

                Validate(std::pmr::memory_resource* __a) : args(__a) {}
                
                Validate(const Validate& other, std::pmr::memory_resource* __a)
                : method(other.method), args(other.args, __a) {}

                Validate(const Validate&) = default;
                Validate(Validate&&) ALIB5_NOEXCEPT = default;
                Validate& operator=(const Validate&) = default;
                Validate& operator=(Validate&&) ALIB5_NOEXCEPT = default;
            };

            std::pmr::vector<Validate> validates;
            
            /**
             * @brief The default value to fall back to if absent or mismatched.
             * 
             * @par Original Comments:
             * English: For a type, the default value is just the default value. For an array, the default value is the default value of its children.
             * Chinese: 对于类型,默认值就是默认值. 对于数组,默认值是子的默认值
             */
            std::optional<dadata_t> default_value;
            
            /**
             * @brief Permitted enumeration values.
             * 
             * @par Original Comments:
             * English: Restricted enum values.
             * Chinese: 限定的枚举值
             */
            std::pmr::unordered_set<
                std::pmr::string,
                detail::TransparentStringHash,
                detail::TransparentStringEqual    
            > enums;

            /**
             * @brief Sub-node rules for Object children.
             * 
             * @par Original Comments:
             * English: Validator does not have high performance requirements, so a more convenient storage method is chosen. Validates child nodes, acts only on Object.
             * Chinese: validate对性能要求没那么高,选择更加方便的存储方式. 校验子节点,仅作用于Object
             */
            std::pmr::unordered_map<std::pmr::string, Node> children;
            
            /**
             * @brief Sub-node rules for Array elements.
             * 
             * @par Original Comments:
             * English: If the type is ARRAY, this is what is validated.
             * Chinese: 类型为ARRAY,那么校验的是这个
             */
            std::pmr::vector<Node> array_subs;
            
            /**
             * @brief Differentiates between strict positional tuples and homogeneous lists.
             * 
             * @par Original Comments:
             * English: Since ARRAY is divided into tuple and list, this serves as an identifier.
             * Chinese: 因为ARRAY分tuple和list,这个作为标识
             */
            bool is_tuple;
            
            /**
             * @brief If true, overrides conflicting data types with the default value instead of erroring out.
             * 
             * @par Original Comments:
             * English: If a default value exists, override when conflicting.
             * Chinese: 如果存在默认值,那么冲突的时候覆盖
             */
            bool override_if_conflict;
            
            Node(std::pmr::memory_resource* __a = std::pmr::get_default_resource())
            : validates(__a), min_length(__a), max_length(__a), children(__a), array_subs(__a), enums(__a) {
                reset();
            }

            Node(const Node& __other, std::pmr::memory_resource* __a)
            : required(__other.required)
            , type_restrict(__other.type_restrict)
            , min_length(__other.min_length, __a)
            , max_length(__other.max_length, __a)
            , validates(__a)
            , default_value(std::nullopt)
            , children(__other.children, __a)
            , array_subs(__other.array_subs, __a)
            , is_tuple(__other.is_tuple)
            , override_if_conflict(__other.override_if_conflict)
            , enums(__other.enums, __a) {
                validates.reserve(__other.validates.size());
                for(auto& __v : __other.validates) validates.emplace_back(__v, __a);
                if(__other.default_value) default_value.emplace(*__other.default_value, __a);
            }

            Node(const Node& __other)
            : Node(__other, __other.validates.get_allocator().resource()) {}
            
            Node(Node&& __other) ALIB5_NOEXCEPT
            : required(__other.required)
            , type_restrict(__other.type_restrict)
            , min_length(std::move(__other.min_length))
            , max_length(std::move(__other.max_length))
            , validates(std::move(__other.validates))
            , default_value(std::move(__other.default_value))
            , children(std::move(__other.children))
            , array_subs(std::move(__other.array_subs))
            , is_tuple(__other.is_tuple)
            , override_if_conflict(__other.override_if_conflict)
            , enums(std::move(__other.enums)) {}

            Node& operator=(Node&&) ALIB5_NOEXCEPT = default;

            /**
             * @brief Explicit assignment operator to prevent compiler deletion.
             */
            Node& operator=(const Node& other) {
                if(this == &other) [[unlikely]] return *this;
                required = other.required;
                type_restrict = other.type_restrict;
                min_length = other.min_length;
                max_length = other.max_length;
                validates = other.validates;
                default_value = other.default_value;
                children = other.children;
                array_subs = other.array_subs;
                is_tuple = other.is_tuple;
                override_if_conflict = other.override_if_conflict;
                enums = other.enums;
                return *this;
            }

            void reset() {
                required = true;
                type_restrict = RNone;
                min_length.set("");
                max_length.set("");
                validates.clear();
                default_value = std::nullopt;
                children.clear();
                array_subs.clear();
                is_tuple = false;
                override_if_conflict = false;
                enums.clear();
            }
        };

        /**
         * @brief Container for tracking validation progress and reporting errors.
         * 
         * @par Original Comments:
         * English: Records errors, operations...
         * Chinese: 记录错误,operations...
         */
        struct ALIB5_API Result {
            bool enable_string_errors;
            bool enable_missing;
            bool success;
            std::pmr::vector<std::pmr::string> recorded_errors;
            
            std::pmr::unordered_set<
                std::pmr::string,
                detail::TransparentStringHash,
                detail::TransparentStringEqual    
            > missings;

            Result(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
            : recorded_errors(__a), missings(__a) {
                reset();
                enable_string_errors = true;
                enable_missing = true;
            }

            void reset() {
                success = true;
                recorded_errors.clear();
                missings.clear();
            }

            /**
             * @brief Formats and logs a validation error.
             */
            template<class... Args> 
            void record_error(std::string_view fmt, Args&&... args) {
                if(enable_string_errors) {
                    auto& str = recorded_errors.emplace_back(
                        std::pmr::string(recorded_errors.get_allocator())
                    );
                    std::vformat_to(
                        std::back_inserter(str),
                        fmt,
                        std::make_format_args(args...)
                    );
                }
                success = false;
            }

            /**
             * @brief Records a missing validation method hook.
             */
            void missing_validate(std::string_view m) {
                if(enable_missing) missings.emplace(m);
            }
        };

        using ValidateMethod = std::function<bool(dadata_t& node, std::pmr::vector<std::pmr::string>& args)>;

        Node root;
        str::StringPool<std::pmr::string> activated_validates_str_pool;
        std::pmr::unordered_map<std::string_view, ValidateMethod> validates;
        std::pmr::memory_resource* allocator;

        Validator(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        : root(__a), activated_validates_str_pool(__a), allocator(__a) {}

        Validator(const dadata_t& d, std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        : root(__a), activated_validates_str_pool(__a), allocator(__a) {
            from_adata(d);
        }

        /**
         * @brief Registers a custom validation function hook.
         * 
         * @par Original Comments:
         * English: Processes validation schemes.
         * Chinese: 处理校验方案
         */
        ValidateMethod& emplace_validate_method(std::string_view name, ValidateMethod method) {
            return validates.emplace(activated_validates_str_pool.get(name), method).first->second;
        }

        /**
         * @brief Parses schema rules from a standard AData document.
         * 
         * @param doc The reference AData tree holding the schema definition.
         * @return std::pmr::string Any parse errors. A string of length 0 means parsing succeeded.
         * 
         * @par Original Comments:
         * English: Returns error information. If the length is 0, it means there is no error information.
         * Chinese: 返回错误信息,如果length为0表示没有错误信息
         */
        std::pmr::string ALIB5_API from_adata(const dadata_t& doc);
        
        /**
         * @brief Executes the validation of a target AData node using the loaded schema.
         * 
         * @param doc The AData node to be checked.
         * @param result Storage payload to gather detailed outcomes and errors.
         * @param ignore_missing Used mostly for C++ struct/class mapping mapping behavior mapping default constructors.
         * @return true If validation succeeds entirely.
         * @return false If any validation rule fails.
         * 
         * @par Original Comments:
         * English: Performs validation, returns false if fails. `ignore_missing` is suitable for C++ class validation, eliminating the need to rewrite classes & mappings, utilizing C++ constructor default value logic.
         * Chinese: 进行校验,returns false if fails. ignore_mussing 适用于C++类校验，从而不需要重新写类&映射，利用了C++构造函数默认值逻辑
         */
        bool ALIB5_API validate(dadata_t& doc, Result& result, bool ignore_missing = false);

        /**
         * @brief Helper wrapper that executes validation and returns the embedded Result object.
         */
        inline Result validate(dadata_t& doc, bool ignore_missing = false) {
            Result r;
            validate(doc, r, ignore_missing);
            return r;
        } 
    };

    namespace debug {
        /**
         * @brief Recursively prints out the structure of a Validator Node for debugging purposes.
         */
        void ALIB5_API print_validator(const Validator::Node& node, int depth = 0, const std::string& label = "");
    }

    using dvalidator_t = Validator;
    using dvalinode_t = Validator::Node;
    using drestriction_t = Validator::Node::TypeRestrict;
    using dresult_t = Validator::Result;

} // namespace alib5

#endif