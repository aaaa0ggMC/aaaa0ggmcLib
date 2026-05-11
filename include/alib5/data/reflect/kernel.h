/**
 * @file kernel.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些支持性质的内容
 * @version 5.0
 * @date 2026-05-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_ADATA_REFLECT_KERNEL
#define ALIB5_ADATA_REFLECT_KERNEL
#include <alib5/data/reflect/attributes.h>
#include <meta>
#include <utility>

namespace alib5{
    namespace detail{
        struct FromADataConfig{
            /// Uses ALogger,so it is required
            bool debug {false};
        };

        struct ToADataConfig{
            /// Uses ALogger,so it is required
            bool debug {false};
        };

        /// annotation_do_if_trait在没有找到trait的时候的返回值
        struct TraitNotFound{};

        constexpr FromADataConfig default_from_adata_config = FromADataConfig();
        constexpr ToADataConfig default_to_adata_config = ToADataConfig();

        /// 判断传入的object是否是一个数组
        template<std::meta::info object>
        constexpr bool target_is_array(){
            return requires {
                { std::forward<decltype([: object :])>([: object :]).size() } -> std::convertible_to<size_t>;
                std::forward<decltype([: object :])>([: object :])[0];
            };
        };

        /// 检查对应的对象是否含有对应trait的annotation
        template<attr::AttributeTraits trait,std::size_t N,std::array<std::meta::info, N> annotations> constexpr
        bool has_annotation_with_trait(){
            template for(
                constexpr auto anno
                :
                annotations
            ){
                constexpr auto anno_type_info = std::meta::type_of(anno);
                using AnnotationType = [: anno_type_info :];
                using AnnotationBaseType = std::decay_t<AnnotationType>;                
                // constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                if constexpr(
                    requires{
                        AnnotationBaseType::attribute_trait;
                    }
                ){
                    if constexpr(AnnotationBaseType::attribute_trait == trait){
                        return true;
                    }
                }
            }

            return false;
        }

        /// 检查annotation_do_if_trait的返回值
        /// 如果没找到返回true,找到了返回false
        template<class T> consteval
        bool not_found_annotation(T && val){
            using TT = std::decay_t<T>;
            return std::is_same_v<TT,TraitNotFound>;
        }

        /// 进行检查，如果找到了，那么就返回转化后得到的value_mapping
        template<attr::AttributeTraits trait,std::size_t N,std::array<std::meta::info, N> annotations> constexpr
        auto annotation_do_if_trait(){
            if constexpr(!has_annotation_with_trait<trait,N,annotations>()){
                return TraitNotFound{};
            }

            template for(
                constexpr auto anno
                :
                annotations
            ){
                constexpr auto anno_type_info = std::meta::type_of(anno);
                using AnnotationType = [: anno_type_info :];
                using AnnotationBaseType = std::decay_t<AnnotationType>;                
                constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                if constexpr(
                    requires{
                        AnnotationBaseType::attribute_trait;
                    }
                ){
                    if constexpr(AnnotationBaseType::attribute_trait == trait){
                        return value_mapping;
                    }
                }
            }
        }

        template<class T,size_t N = 0,std::array<std::meta::info,N> annotations> constexpr
        bool has_annotation_of_type(){
            template for(
                constexpr auto anno
                :
                annotations
            ){
                constexpr auto a_type_info = std::meta::type_of(anno);
                using AnnoType = [: a_type_info :];

                if constexpr(
                    std::is_same_v<
                        T,
                        std::decay_t<AnnoType>
                    >
                ){
                    return true;
                }
            }
            return false;
        }

        template <typename T, std::size_t N,class K>
        constexpr auto ranges_to_array(K && val) -> std::array<T, N> {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>){
                return std::array<T, N>{ std::forward<K>(val)[Is]... };
            }(std::make_index_sequence<N>{});
        }
    }
}
#endif