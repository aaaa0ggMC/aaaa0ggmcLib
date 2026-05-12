/**
 * @file kernel.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 注解支持
 * @version 5.0
 * @date 2026-05-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_ADATA_REFLECT_ATTR
#define ALIB5_ADATA_REFLECT_ATTR
#include <string_view>
#include <limits>
#include <meta>
#include <tuple>

namespace alib5{
    namespace detail{
        ///@brief 大小可以在编译期间自动fit的类型
        template<std::size_t N>
        struct fixed_string {
            char data[N]{};

            constexpr fixed_string(const char (&str)[N]){
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


        ///@brief STuple,一个满足StructuralType的在这里替代std::tuple的东西
        template<typename... Args>
        struct STuple;

        template<>
        struct STuple<>{};

        template<typename T, typename... Args>
        struct STuple<T, Args...> {
            T value;
            [[no_unique_address]] STuple<Args...> rest;
            
            constexpr STuple(T arg,Args... args):value(arg),rest(args...){}

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

        template<typename T,typename... Args>
        STuple(T,Args...) -> STuple<T,Args...>;

        STuple() -> STuple<>;
    }

    namespace attr {
        /// 用于attr::range，表示这个变量没有进行限制
        constexpr double no_range = std::numeric_limits<double>::infinity();

        /// 标记trait状态
        enum class AttributeTraits{
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
            GeneralSkip
        };

/// MultiUse (Serialize[S],Deserialize[D] & Schema[Sc]) ////
        
        /// [SDSc] 重新映射变量的名字
        template<detail::fixed_string S>
        struct rename {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::Rename;
            
            /// 新的名字
            constexpr static std::string_view new_name() {
                return S.view();
            }
        };

        /// [SDSc] 对于 container<...> ，允许将annotation穿透给内部数据
        template<class... Ts>
        struct element_attr {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::InnerAttributes;
            detail::STuple<Ts...> objects;

            constexpr element_attr(Ts... os):
            objects(os...){}

            /// 从tuple变成std::meta::info
            consteval std::array<std::meta::info,sizeof...(Ts)>
            to_annotation_array() const {
                constexpr size_t N = sizeof...(Ts);
                return [&]<std::size_t... Is>(std::index_sequence<Is...>){
                    return std::array<std::meta::info, N>{ 
                        std::meta::reflect_constant(objects.template get<Is>())... 
                    };
                }(std::make_index_sequence<N>());
            }
        };

        /// 通用skip
        struct skip{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::GeneralSkip;
        };
        
//// Deserialize Only ///
namespace deseri{
        /// 反序列化直接跳过这个
        struct skip{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::DeseriSkip;
        };

}


//// Serialize Only ////
namespace seri{
        /// 转换成adata对象时，如果转换结果为字符串并且为""，那么这个变量会被舍弃
        /// 注意，如果你序列化后校验schema的话，schema校验可能不通过(比如，没有这个变量)
        /// 因为omit_empty并不影响schema的检测
        struct omit_empty {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::OmitEmpty;
        };

        /// 序列化直接跳过这个
        struct skip{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::SeriSkip;
        };

}
//// Schema Only ////
namespace schema{
        /// 用在schema里面阻止对这个进行schema生成
        struct skip {
            constexpr static AttributeTraits attribute_trait = AttributeTraits::SchemaSkip;
        };

        /// 用在schema里面限制范围
        struct range{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::SchemaRange;

            double min;
            double max;
        };

        /// 用在schema里面进行validate
        template<detail::fixed_string func_name>
        struct validate{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::SchemaValidate;

            constexpr static std::string_view validate_function() {
                return func_name.view();
            }
        };

        /// 如何食用这个玩意儿，正如你所见，它只接受引用
        /// 因此你需要自己找个地方constexpr定义好数据后传入进来
        /// 如果使用T而不是T&,很多类型会因为不是structural type被拒绝
        /// 这个是目前我觉得最好的方案了，支持能constexpr的所有类型
        template<class T>
        struct default_value{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::DefaultValue;
            T & value;
        };

        /// 支持 validate func ( arg1 arg2 ... )的生成
        template<detail::fixed_string func_name,detail::fixed_string... args>
        struct validate_args{
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