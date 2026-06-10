/**
 * @file attributes.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Annotation support for AData Reflection.
 * 
 * \par Original Comment
 * English: Annotation support.
 * Chinese: 注解支持
 * 
 * @version 5.0
 * @date 2026-05-11
 * @copyright Copyright (c) 2026
 */
#ifndef ALIB5_ADATA_REFLECT_ATTR
#define ALIB5_ADATA_REFLECT_ATTR
#include <string_view>
#include <limits>
#include <meta>
#include <tuple>

namespace alib5 {
    namespace detail {
        
        /**
         * @brief A type whose size can be automatically fitted at compile time.
         * 
         * \par Original Comment
         * English: A type whose size can be automatically fitted at compile time.
         * Chinese: 大小可以在编译期间自动fit的类型
         * 
         * @tparam N Size of the string.
         */
        template<std::size_t N>
        struct fixed_string {
            char data[N]{};

            constexpr fixed_string(const char (&str)[N]) {
                for (std::size_t i = 0; i < N; ++i) {
                    data[i] = str[i];
                }
            }

            constexpr std::string_view view() const {
                return std::string_view{data, N - 1};
            }

            constexpr const char * c_str() const {
                return data;
            }
        };
        
        template <std::size_t N>
        fixed_string(const char (&)[N]) -> fixed_string<N>;


        /**
         * @brief STuple, a type that satisfies StructuralType to replace std::tuple here.
         * 
         * \par Original Comment
         * English: STuple, something that satisfies StructuralType to replace std::tuple here.
         * Chinese: STuple,一个满足StructuralType的在这里替代std::tuple的东西
         * 
         * @tparam Args Types of the tuple elements.
         */
        template<typename... Args>
        struct STuple;

        template<>
        struct STuple<> {};

        template<typename T, typename... Args>
        struct STuple<T, Args...> {
            T value;
            [[no_unique_address]] STuple<Args...> rest;
            
            constexpr STuple(T arg, Args... args): value(arg), rest(args...) {}

            template<std::size_t N>
            constexpr const auto& get() const {
                if constexpr(N == 0) return value;
                else return rest.template get<N - 1>();
            }

            template<std::size_t N>
            constexpr auto& get() {
                if constexpr(N == 0) return value;
                else return rest.template get<N - 1>();
            }

        #if __cplusplus >= 202302L
            template<std::size_t N>
            constexpr const auto& operator[]() const {
                return get<N>();
            }
        #endif
        };

        template<typename T, typename... Args>
        STuple(T, Args...) -> STuple<T, Args...>;

        STuple() -> STuple<>;
    }

    namespace attr {
        
        /**
         * @brief Used for attr::range, indicating this variable has no limits.
         * 
         * \par Original Comment
         * English: Used for attr::range, indicating this variable is not restricted.
         * Chinese: 用于attr::range，表示这个变量没有进行限制
         */
        constexpr double no_range = std::numeric_limits<double>::infinity();

        /**
         * @brief Enum marking the trait status.
         * 
         * \par Original Comment
         * English: Mark trait status.
         * Chinese: 标记trait状态
         */
        enum class AttributeTraits {
            ValidateWithArgs,
            DefaultValue,
            InnerAttributes,
            Rename,
            SchemaSkip,
            SchemaRange,
            SchemaValidate,
            OmitEmpty,
            SeriSkip,
            DeseriSkip,
            GeneralSkip,
            OverrideIfConflict,
            Optional,
            Extra,
        };

        // ================= MultiUse (Serialize[S], Deserialize[D] & Schema[Sc]) =================
        
        /**
         * @brief [SDSc] Remap the name of the variable.
         * 
         * \par Original Comment
         * English: Remap the name of the variable. / New name.
         * Chinese: [SDSc] 重新映射变量的名字 / 新的名字
         */
        template<detail::fixed_string S>
        struct rename {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::Rename;
            
            constexpr static std::string_view new_name() {
                return S.view();
            }
        };

        /**
         * @brief [SDSc] For container<...>, allows annotations to penetrate to internal data.
         * 
         * \par Original Comment
         * English: For container<...>, allows annotations to penetrate to internal data. / Change from tuple to std::meta::info
         * Chinese: [SDSc] 对于 container<...> ，允许将annotation穿透给内部数据 / 从tuple变成std::meta::info
         */
        template<class... Ts>
        struct element_attr {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::InnerAttributes;
            detail::STuple<Ts...> objects;

            constexpr element_attr(Ts... os): objects(os...) {}

            consteval std::array<std::meta::info, sizeof...(Ts)>
            to_annotation_array() const {
                constexpr size_t N = sizeof...(Ts);
                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    return std::array<std::meta::info, N>{ 
                        std::meta::reflect_constant(objects.template get<Is>())... 
                    };
                }(std::make_index_sequence<N>());
            }
        };

        /**
         * @brief [SDSc] General skip for serialization, deserialization, and schema.
         * 
         * \par Original Comment
         * English: General skip.
         * Chinese: [SDSc] 通用skip
         */
        struct skip {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::GeneralSkip;
        };

        // ================= Deserialize Only =================
        namespace deseri {
            
            /**
             * @brief Directly skip this during deserialization.
             * 
             * \par Original Comment
             * English: Skip this directly when deserializing.
             * Chinese: 反序列化直接跳过这个
             */
            struct skip {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::DeseriSkip;
            };

        }

        // ================= Serialize Only =================
        namespace seri {
            
            /**
             * @brief When converting to an adata object, if the result is an empty string (""), the variable is discarded.
             * 
             * \par Original Comment
             * English: When converted to an adata object, if the conversion result is a string and is "", then this variable will be discarded. Note that if you check the schema after serialization, the schema check may not pass (for example, this variable is missing) because omit_empty does not affect schema detection.
             * Chinese: 转换成adata对象时，如果转换结果为字符串并且为""，那么这个变量会被舍弃 注意，如果你序列化后校验schema的话，schema校验可能不通过(比如，没有这个变量) 因为omit_empty并不影响schema的检测
             */
            struct omit_empty {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::OmitEmpty;
            };

            /**
             * @brief Directly skip this during serialization.
             * 
             * \par Original Comment
             * English: Skip this directly when serializing.
             * Chinese: 序列化直接跳过这个
             */
            struct skip {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::SeriSkip;
            };

        }
        
        // ================= Schema Only =================
        namespace schema {
            
            /**
             * @brief Mark as optional.
             * 
             * \par Original Comment
             * English: Mark this as optional.
             * Chinese: 标注这是optional的
             */
            struct optional {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::Optional;
            };

            /**
             * @brief Additional rules.
             * 
             * \par Original Comment
             * English: Extra rules.
             * Chinese: 额外规则
             */
            template<detail::fixed_string S>
            struct extra {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::Extra;
            
                constexpr static std::string_view extra_restraints() {
                    return S.view();
                }
            };
            
            /**
             * @brief Ignore and override with default value if there's a conflict (e.g., data type, range). Requires default value to exist.
             * 
             * \par Original Comment
             * English: If the data type conflicts, range conflicts...... ignore it and overwrite it with the default value. Requires a default value to exist.
             * Chinese: 如果数据类型冲突，range冲突......忽略然后覆盖为默认值 要求默认值存在
             */
            struct fallback {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::OverrideIfConflict;
            };

            /**
             * @brief Prevent schema generation for this field in the schema.
             * 
             * \par Original Comment
             * English: Used in schema to prevent schema generation for this.
             * Chinese: 用在schema里面阻止对这个进行schema生成
             */
            struct skip {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::SchemaSkip;
            };

            /**
             * @brief Limit range in schema.
             * 
             * \par Original Comment
             * English: Used in schema to restrict range.
             * Chinese: 用在schema里面限制范围
             */
            struct range {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::SchemaRange;

                double min;
                double max;
            };

            /**
             * @brief Validate in schema using a function.
             * 
             * \par Original Comment
             * English: Used in schema to perform validate.
             * Chinese: 用在schema里面进行validate
             */
            template<detail::fixed_string func_name>
            struct validate {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::SchemaValidate;

                constexpr static std::string_view validate_function() {
                    return func_name.view();
                }
            };

            /**
             * @brief Default value injection.
             * 
             * \par Original Comment
             * English: How to consume this stuff? As you can see, it only accepts references. Therefore, you need to find a place to define the data with constexpr and then pass it in. If you use T instead of T&, many types will be rejected because they are not structural types. This is the best solution I can think of right now; it supports all types that can be constexpr.
             * Chinese: 如何食用这个玩意儿，正如你所见，它只接受引用 因此你需要自己找个地方constexpr定义好数据后传入进来 如果使用T而不是T&,很多类型会因为不是structural type被拒绝 这个是目前我觉得最好的方案了，支持能constexpr的所有类型
             */
            template<class T>
            struct default_value {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::DefaultValue;
                T & value;
            };

            /**
             * @brief Supports generation of validation function with arguments.
             * 
             * \par Original Comment
             * English: Support validate func (arg1 arg2 ...) generation.
             * Chinese: 支持 validate func ( arg1 arg2 ... )的生成
             */
            template<detail::fixed_string func_name, detail::fixed_string... args>
            struct validate_args {
                constexpr static AttributeTraits attribute_trait = AttributeTraits::ValidateWithArgs;
                constexpr static std::size_t ArgCount = sizeof...(args);

                constexpr static std::string_view validate_function() {
                    return func_name.view();
                }

                template<size_t N>
                constexpr static auto& get_arg() {
                    static constexpr auto t = std::make_tuple(&args...);
                    return *std::get<N>(t);
                }
            };

        }
    }
}

#endif