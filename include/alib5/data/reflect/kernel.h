/**
 * @file kernel.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Kernel components for reflection support.
 * 
 * \par Original Comment
 * English: Some supporting contents.
 * Chinese: 一些支持性质的内容
 * 
 * @version 5.0
 * @date 2026-05-11
 * @copyright Copyright (c) 2026
 */
#ifndef ALIB5_ADATA_REFLECT_KERNEL
#define ALIB5_ADATA_REFLECT_KERNEL
#include <alib5/data/reflect/attributes.h>
#include <alib5/autil.h>
#include <meta>
#include <utility>

namespace alib5 {
    namespace detail {
        
        /**
         * @brief Configuration for converting from AData.
         */
        struct FromADataConfig {
            /**
             * @brief Debug flag.
             * 
             * \par Original Comment
             * English: Uses ALogger, so it is required.
             * Chinese: Uses ALogger, so it is required.
             */
            bool debug {false};
        };

        /**
         * @brief Configuration for converting to AData.
         */
        struct ToADataConfig {
            /**
             * @brief Debug flag.
             * 
             * \par Original Comment
             * English: Uses ALogger, so it is required.
             * Chinese: Uses ALogger, so it is required.
             */
            bool debug {false};
        };

        /**
         * @brief Empty struct representing that a trait was not found.
         * 
         * \par Original Comment
         * English: The return value when annotation_do_if_trait does not find the trait.
         * Chinese: annotation_do_if_trait在没有找到trait的时候的返回值
         */
        struct TraitNotFound {};

        constexpr FromADataConfig default_from_adata_config = FromADataConfig();
        constexpr ToADataConfig default_to_adata_config = ToADataConfig();

        /**
         * @brief Checks if the provided object is an array.
         * 
         * \par Original Comment
         * English: Check if the passed object is an array.
         * Chinese: 判断传入的object是否是一个数组
         * 
         * @tparam object Reflection info of the object.
         * @return constexpr bool True if the object behaves like an array.
         */
        template<std::meta::info object>
        constexpr bool target_is_array() {
            return requires {
                { std::forward<decltype([: object :])>([: object :]).size() } -> std::convertible_to<size_t>;
                std::forward<decltype([: object :])>([: object :])[0];
            };
        }

        /**
         * @brief Checks if the corresponding object has an annotation with the specified trait.
         * 
         * \par Original Comment
         * English: Check if the corresponding object contains the annotation of the corresponding trait.
         * Chinese: 检查对应的对象是否含有对应trait的annotation
         * 
         * @tparam trait The trait to check for.
         * @tparam N The number of annotations.
         * @tparam annotations The array of annotations.
         * @return constexpr bool True if found, false otherwise.
         */
        template<
            attr::AttributeTraits trait,
            std::size_t N,
            std::array<
                std::meta::info,
                N
            > annotations
        > constexpr
        bool has_annotation_with_trait() {
            template for(constexpr auto anno : annotations) {
                constexpr auto anno_type_info = std::meta::type_of(anno);
                using AnnotationType = [: anno_type_info :];
                using AnnotationBaseType = std::decay_t<AnnotationType>;                

                if constexpr(
                    requires {
                        AnnotationBaseType::attribute_trait;
                    }
                ) {
                    if constexpr(AnnotationBaseType::attribute_trait == trait) {
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * @brief Checks the return value of annotation_do_if_trait.
         * 
         * \par Original Comment
         * English: Check the return value of annotation_do_if_trait. Returns true if not found, false if found.
         * Chinese: 检查annotation_do_if_trait的返回值 如果没找到返回true,找到了返回false
         * 
         * @tparam T Type to check.
         * @param val Value to check.
         * @return consteval bool True if it is TraitNotFound.
         */
        template<class T> consteval
        bool not_found_annotation(T && val) {
            using TT = std::decay_t<T>;
            return std::is_same_v<TT, TraitNotFound>;
        }

        /**
         * @brief Performs a check, and if the trait is found, returns the mapped value.
         * 
         * \par Original Comment
         * English: Perform a check, and if found, return the converted value_mapping.
         * Chinese: 进行检查，如果找到了，那么就返回转化后得到的value_mapping
         * 
         * @tparam trait The trait to look for.
         * @tparam N The number of annotations.
         * @tparam annotations The array of annotations.
         * @return constexpr auto The value mapping or TraitNotFound{}.
         */
        template<
            attr::AttributeTraits trait,
            std::size_t N,
            std::array<
                std::meta::info,
                N
            > annotations
        > constexpr
        auto annotation_do_if_trait() {
            if constexpr(!has_annotation_with_trait<trait, N, annotations>()) {
                return TraitNotFound{};
            }

            template for(constexpr auto anno : annotations) {
                constexpr auto anno_type_info = std::meta::type_of(anno);
                using AnnotationType = [: anno_type_info :];
                using AnnotationBaseType = std::decay_t<AnnotationType>;                
                constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                if constexpr(
                    requires {
                        AnnotationBaseType::attribute_trait;
                    }
                ) {
                    if constexpr(AnnotationBaseType::attribute_trait == trait) {
                        return value_mapping;
                    }
                }
            }
        }

        /**
         * @brief Converts various range-like types to std::array.
         * 
         * \par Original Comment
         * English: Converts various range-like types to std::array, similar to what std::define_static_array does. But std converts to span, which cannot pass data.
         * Chinese: 把各种类ranges类型转换成std::array,类似std::define_static_array干的事情 但是std是转换成span,无法进行数据传递
         * 
         * @tparam T Type of the array elements.
         * @tparam N Size of the array.
         * @tparam K The range-like type.
         * @param val The range object.
         * @return constexpr auto The resulting std::array.
         */
        template<
            typename T, 
            std::size_t N,
            class K
        >
        constexpr auto ranges_to_array(K && val) -> std::array<T, N> {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return std::array<T, N>{ std::forward<K>(val)[Is]... };
            }(std::make_index_sequence<N>{});
        }

        /**
         * @brief Gets a static unordered map for enum mapping.
         * 
         * \par Original Comment
         * English: Get a static umap.
         * Chinese: 获取一个静态的umap
         * 
         * @tparam T Enum type.
         * @return const std::unordered_map<std::string, T, detail::TransparentStringHash, detail::TransparentStringEqual>& 
         */
        template<class T>
        requires std::is_enum_v<T>
        const std::unordered_map<
            std::string,
            T,
            detail::TransparentStringHash,
            detail::TransparentStringEqual
        >& get_enum_mapper() {
            static std::unordered_map<
                std::string,
                T,
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > data;
            static bool init = [&] {
                template for(
                    constexpr auto item :
                    std::define_static_array(std::meta::enumerators_of(
                        ^^T
                    ))
                ) {
                    data.emplace(std::meta::identifier_of(item), [: item :]);
                }
                return true;
            }();
            return data;
        }

        /**
         * @brief Gets a reverse static unordered map mapping enums to strings.
         * 
         * \par Original Comment
         * English: Look at it in reverse.
         * Chinese: 反着看
         * 
         * @tparam T Enum type.
         * @return const std::unordered_map<T, std::string>& 
         */
        template<class T>
        requires std::is_enum_v<T>
        const std::unordered_map<
            T,
            std::string
        >& get_enum_string_mapper() {
            static std::unordered_map<
                T,
                std::string
            > data;
            static bool init = [&] {
                template for(
                    constexpr auto item :
                    std::define_static_array(std::meta::enumerators_of(
                        ^^T
                    ))
                ) {
                    data.emplace([: item :], std::meta::identifier_of(item));
                }
                return true;
            }();
            return data;
        }

        /**
         * @brief Checks if a specific type exists in the annotation list.
         * 
         * \par Original Comment
         * English: Unused, but kept anyway. Check if a specific type exists in the annotation list.
         * Chinese: 虽然没用上，但还是留着 检查annotation列表中是否存在特定的类型
         * 
         * @tparam T The type to check for.
         * @tparam N The number of annotations.
         * @tparam annotations The array of annotations.
         * @return constexpr bool True if the type exists.
         */
        template<
            class T,
            size_t N = 0,
            std::array<
                std::meta::info,
                N
            > annotations
        > constexpr
        bool has_annotation_of_type() {
            template for(constexpr auto anno : annotations) {
                constexpr auto a_type_info = std::meta::type_of(anno);
                using AnnoType = [: a_type_info :];

                if constexpr(
                    std::is_same_v<
                        T,
                        std::decay_t<AnnoType>
                    >
                ) {
                    return true;
                }
            }
            return false;
        }
    }
}
#endif