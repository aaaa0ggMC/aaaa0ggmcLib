#ifndef ALIB5_DATA_REFLECT
#define ALIB5_DATA_REFLECT
#include <alib5/data/kernel.h>
#include <alib5/autil.h>
#include <limits.h>
#include <meta>


namespace alib5{
    constexpr size_t conf_mapping_maximum_size = 64;
    constexpr size_t conf_single_arg_maximum_size = 128;

    namespace attr{
        constexpr double no_range = std::numeric_limits<double>::infinity();

        enum class AttributeTraits{
            ValidateWithArgs
        };

        struct mapping {
            char new_name [conf_mapping_maximum_size];
        };
        struct omit_empty {};
        struct pass_attributes {};

        struct range{
            double min;
            double max;
        };

        struct validate{
            char validate_function [ conf_mapping_maximum_size ];
        };

        template<size_t N>
        struct validate_with_args{
            constexpr static AttributeTraits attribute_trait = AttributeTraits::ValidateWithArgs;
            constexpr static size_t ArgCount = N;

            char validate_function [ conf_mapping_maximum_size ];
            char args[N][ conf_single_arg_maximum_size ];
        };
    }

    template<class InT> 
    void from_adata(InT & fill_data,const AData & root);

    template<class InT> 
    InT from_adata(const AData & root);

    template<class InT> 
    AData to_adata(InT && base);

    template<class T>
    AData generate_schema();

    namespace detail{
        struct FromADataConfig{
            /// Uses ALogger,so it is required
            bool debug {false};
        };

        struct ToADataConfig{
            /// Uses ALogger,so it is required
            bool debug {false};
        };

        constexpr FromADataConfig default_from_adata_config = FromADataConfig();
        constexpr ToADataConfig default_to_adata_config = ToADataConfig();

        template<std::meta::info object>
        constexpr bool target_is_array(){
            return requires {
                { std::forward<decltype([: object :])>([: object :]).size() } -> std::convertible_to<size_t>;
                std::forward<decltype([: object :])>([: object :])[0];
            };
        };

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

        /// 从数据中加载
        template<
            FromADataConfig cfg = default_from_adata_config,
            size_t N = 0,
            std::array<std::meta::info,N> annotations = {},
            class LoggerType = std::nullptr_t,
            class InT
        > 
        void _from_adata(InT & fill_data,const AData & root,LoggerType * debug_logger = nullptr){   
            constexpr std::meta::access_context context = std::meta::access_context::unchecked(); 
            
            if constexpr(
                IsNodeValue<InT>
            ){
                panic_if(
                    !root.is_value(),
                    "Node is not a value!"
                );

                fill_data = root.to<InT>();

                if constexpr(cfg.debug){
                    if(debug_logger) [[likely]] {
                        *debug_logger << "Type  : " << std::meta::display_string_of(^^InT) << fls;
                        *debug_logger << "Value : " << fill_data << "\n" << fls;
                    }
                }
            }else if constexpr(
                requires{
                    typename InT::value_type;
                    typename InT::value_type();
                }

                &&

                requires(typename InT::value_type & val){
                    fill_data.push_back(val);
                    { fill_data.back() } -> std::convertible_to<typename InT::value_type &>;
                    }
            ){
                panic_if(
                    !root.is_array(),
                    "Node is not an array!"
                );
                auto & arr = root.array();

                // 是否支持提前reserve
                if constexpr(requires{
                    fill_data.reserve((size_t)1);
                }){
                    fill_data.reserve(arr.size());
                }

                using ValueType = typename InT::value_type;

                if constexpr(cfg.debug){
                    if(debug_logger) [[likely]] {
                        *debug_logger << "Type      : " << std::meta::display_string_of(^^InT) << fls;
                        *debug_logger << "ValueType : " << std::meta::display_string_of(^^ValueType) << "\n" << fls;
                    }
                }

                for(size_t i = 0;i < arr.size();++i){
                    fill_data.push_back(
                        ValueType()
                    );

                    if constexpr(
                        detail::has_annotation_of_type<attr::pass_attributes,N,annotations>()
                    ){
                        _from_adata<
                            cfg,N,annotations
                        >(fill_data.back(),arr[i],debug_logger);
                    }else{
                        _from_adata<cfg>(fill_data.back(),arr[i],debug_logger);
                    }
                }

            }else if constexpr(
                std::meta::is_class_type(^^InT)
            ){
                panic_if(
                    !root.is_object(),
                    "Node is not an object!"
                );

                template for(
                    constexpr auto item :
                    std::define_static_array(
                        std::meta::nonstatic_data_members_of(^^InT,context)
                    )
                ){
                    constexpr auto original_name = std::meta::identifier_of(item);

                    std::string_view name = original_name;

                    constexpr static auto child_annotations = std::define_static_array(
                        std::meta::annotations_of(item)
                    );
                    constexpr static auto array_annotations = detail::ranges_to_array<std::meta::info,child_annotations.size()>(
                        child_annotations
                    );

                    // 检测annotation
                    template for(
                        constexpr auto anno
                        :
                        child_annotations
                    ){
                        constexpr auto anno_type_info = std::meta::type_of(anno);
                        using AnnotationType = [: anno_type_info :];
                        using AnnotationBaseType = std::decay_t<AnnotationType>;                
                        constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                        if constexpr(std::is_same_v<AnnotationBaseType,attr::mapping>){
                            if(value_mapping.new_name != nullptr){
                                name = value_mapping.new_name;
                            }
                        }
                    }

                    if constexpr(cfg.debug){
                        if(debug_logger) [[likely]] {
                            *debug_logger << "Type         : " << std::meta::display_string_of(^^InT) << fls;
                            *debug_logger << "OriginalName : " << original_name << fls;
                            *debug_logger << "MappingName  : " << name << "\n" << fls;
                            *debug_logger << "ExistInData  : " << root.object().contains(name) << "\n" << fls;
                        }
                    }

                    if(root.object().contains(name)){
                        _from_adata<
                            cfg,
                            array_annotations.size(),
                            array_annotations
                        >(fill_data.[: item :], root[name],debug_logger);
                    }
                }

            }else{
                static_assert(std::meta::is_class_type(^^InT),"Unsupported Structure!");
            }
        }

        template<
            ToADataConfig cfg = default_to_adata_config,
            size_t N = 0,
            std::array<std::meta::info,N> annotations = {},
            class LoggerType = std::nullptr_t,
            class InT
        > 
        AData _to_adata(InT && base,LoggerType * debug_logger = nullptr){
            using T = std::decay_t<InT>;
            constexpr std::meta::info t_info = ^^T;
            constexpr std::meta::access_context context = std::meta::access_context::unchecked();

            AData root;

            constexpr auto target_is_direct = [&]() consteval -> bool {
                return requires{
                    root = std::forward<InT>(base);
                };
            };
            constexpr auto target_is_object = [&](std::meta::info info) consteval -> bool {
                return std::meta::is_class_type(info);
            };

            if constexpr( target_is_direct() ){
                root = std::forward<InT>(base);

                if constexpr(cfg.debug){
                    if(debug_logger) [[likely]] {
                        *debug_logger << "Type  : " << std::meta::display_string_of(^^InT) << fls;
                        *debug_logger << "Value : " << base << "\n" << fls;
                    }
                }
            }else if constexpr( detail::target_is_array< ^^base >() ){
                root.set<darray_t>();
                size_t size = std::forward<InT>(base).size();
                
                if(size)root.array().reserve(size);

                if constexpr(cfg.debug){
                    if(debug_logger) [[likely]] {
                        bool has_pass = false;
                        if constexpr(
                            detail::has_annotation_of_type<attr::pass_attributes,N,annotations>()
                        ){
                            has_pass = true;
                        }
                        
                        

                        *debug_logger << "Type          : " << std::meta::display_string_of(^^InT) << fls;
                        if constexpr(
                            requires{
                                typename InT::value_type;
                            }
                        ){
                            using ValueType = typename InT::value_type;
                            *debug_logger << "ValueType     : " << std::meta::display_string_of(^^ValueType) << fls;
                        }
                        *debug_logger << "PassAttribute : " << has_pass << "\n" << fls;
                    }
                }

                for(size_t i = 0;i < size;++i){
                    if constexpr(
                        detail::has_annotation_of_type<attr::pass_attributes,N,annotations>()
                    ){
                        root[i] = std::move(
                            _to_adata<cfg,N,annotations>(
                                std::forward<InT>(base)[i],debug_logger
                            )
                        );
                    }else{
                        root[i] = std::move(
                            _to_adata<cfg>(
                                std::forward<InT>(base)[i],debug_logger
                            )
                        );
                    }
                }

                
            }else if constexpr( target_is_object(t_info) ){
                constexpr static auto items = 
                std::define_static_array(
                    std::meta::nonstatic_data_members_of(
                        t_info,
                        context
                    )
                );
                constexpr size_t item_size = items.size();

                root.set<dobject_t>();
                root.object().reserve(
                    item_size
                );

                template for(
                    constexpr auto item
                    :
                    items
                ){
                    // 检查是不是static的
                    constexpr auto item_type = std::meta::type_of(item);
                    constexpr auto original_name = std::meta::identifier_of(item);

                    std::string_view name = original_name;

                    constexpr static auto child_annotations = std::define_static_array(
                        std::meta::annotations_of(item)
                    );
                    constexpr static auto array_annotations = detail::ranges_to_array<std::meta::info,child_annotations.size()>(
                        child_annotations
                    );

                    // 检测annotation
                    template for(
                        constexpr auto anno
                        :
                        child_annotations
                    ){
                        constexpr auto anno_type_info = std::meta::type_of(anno);
                        using AnnotationType = [: anno_type_info :];
                        using AnnotationBaseType = std::decay_t<AnnotationType>;
                        
                        constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                        if constexpr(std::is_same_v<AnnotationBaseType,attr::mapping>){
                            if(value_mapping.new_name != nullptr){
                                name = value_mapping.new_name;
                            }
                        }
                    }

                    constexpr bool need_omit = detail::has_annotation_of_type<attr::omit_empty,array_annotations.size(),array_annotations>();
                    // 如果能cast就先cast
                    if constexpr(
                        requires{
                            root[name] = std::forward<InT>(base).[: item :];
                        }
                    ){
                        auto & node = root[name];

                        node = std::forward<InT>(base).[: item :];               
                        
                        if constexpr(cfg.debug){
                            if(debug_logger) [[likely]] {
                                *debug_logger << "Type         : " << std::meta::display_string_of(^^InT) << fls;
                                *debug_logger << "OriginalName : " << original_name << fls;
                                *debug_logger << "MappingName  : " << name << fls;
                                *debug_logger << "Value        : " << node.to<std::string_view>() << fls;
                                *debug_logger << "OmitEmpty    : " << need_omit << fls;
                            }
                        }

                        bool omitted = false;
                        if constexpr(
                            need_omit
                        ){
                            // 因为我不知道数据格式化进去后是啥玩意儿。。。。
                            // 虽然是基础数据，但是还是需要额外的步骤
                            if(
                                node.is_value() &&
                                node.value().get_type() == Value::Type::STRING &&
                                node.to<std::string_view>() == ""
                            ){
                                root.object().remove(name);
                                omitted = true;
                                // 这后面node1就失效了
                            }
                        }

                        if constexpr(cfg.debug){
                            if(debug_logger) [[likely]] {
                                *debug_logger << "Omitted      : " << omitted << "\n" << fls;
                            }
                        }
                    }else{
                        bool cond = true;
                        auto & target = base.[: item :];

                        if constexpr(
                            detail::target_is_array< ^^target >() &&
                            need_omit
                        ){
                            if(!std::forward<InT>(base).[: item :].size()){
                                cond = false;
                            }
                        }

                        if constexpr(cfg.debug){
                            if(debug_logger) [[likely]] {
                                *debug_logger << "Type         : " << std::meta::display_string_of(^^InT) << fls;
                                *debug_logger << "OriginalName : " << original_name << fls;
                                *debug_logger << "MappingName  : " << name << fls;
                                *debug_logger << "OmitEmpty    : " << need_omit << fls;
                            }
                        }

                        if(cond){
                            root[name] = std::move(
                                _to_adata<
                                    cfg,
                                    child_annotations.size(),
                                    array_annotations
                                >(
                                    std::forward<InT>(base).[: item :],
                                    debug_logger
                                )
                            );

                            if constexpr(cfg.debug){
                                if(debug_logger) [[likely]] {
                                    *debug_logger << "Omitted      : " << true << "\n" << fls;
                                }
                            }
                        }else{
                            if constexpr(cfg.debug){
                                if(debug_logger) [[likely]] {
                                    *debug_logger << "Omitted      : " << false << "\n" << fls;
                                }
                            }
                        }
                    }
                }
            }else{
                static_assert(
                    std::meta::is_class_type(t_info),
                    "Unsupported Structure!"
                );
            }

            return root;
        }


        template<class T,size_t N = 0,std::array<std::meta::info,N> annotations = {}>
        AData _generate_schema(){
            using decayed_T = std::decay_t<T>;
            constexpr auto context = std::meta::access_context::unchecked(); 

            AData root;

            bool add_constaint = true;

            if constexpr(
                std::is_integral_v<std::decay_t<T>>
            ){
                root[0] = "TYPE INT";
            }else if constexpr(
                std::is_floating_point_v<std::decay_t<T>>
            ){
                root[0] = "TYPE DOUBLE";
            }else if constexpr(
                std::is_same_v<std::decay_t<T>,bool>
            ){
                root[0] = "TYPE BOOL";
            }else if constexpr(
                IsStringLike<std::decay_t<T>> || IsStringLike<T>
            ){
                root[0] = "TYPE STRING";
            }else if constexpr(
                requires{
                    { std::declval<decayed_T>().size() } -> std::convertible_to<std::size_t>;
                    std::declval<decayed_T>()[0];
                    typename decayed_T::value_type;
                }
            ){
                root[0] = "TYPE ARRAY";
                if constexpr(
                    detail::has_annotation_of_type<attr::pass_attributes,N,annotations>()
                ){
                    root[1] = std::move(
                        _generate_schema<typename decayed_T::value_type,N,annotations>()
                    );
                }else{
                    root[1] = std::move(
                        _generate_schema<typename decayed_T::value_type>()
                    );
                }
            }else if constexpr(
                std::meta::is_class_type(^^decayed_T)
            ){
                add_constaint = false;

                template for(
                    constexpr auto item
                    :
                    std::define_static_array(
                        std::meta::nonstatic_data_members_of(^^decayed_T,context)
                    )
                ){
                    constexpr auto original_name = std::meta::identifier_of(item);
                    std::string_view name = original_name;
                    using ValueType = [: std::meta::type_of(item) :];

                    constexpr static auto child_annotations = std::define_static_array(
                        std::meta::annotations_of(item)
                    );
                    constexpr static auto array_annotations = detail::ranges_to_array<std::meta::info,child_annotations.size()>(
                        child_annotations
                    );

                    // 检测annotation
                    template for(
                        constexpr auto anno
                        :
                        child_annotations
                    ){
                        constexpr auto anno_type_info = std::meta::type_of(anno);
                        using AnnotationType = [: anno_type_info :];
                        using AnnotationBaseType = std::decay_t<AnnotationType>;                
                        constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                        if constexpr(std::is_same_v<AnnotationBaseType,attr::mapping>){
                            if(value_mapping.new_name != nullptr){
                                name = value_mapping.new_name;
                            }
                        }
                    }

                    root[name] = std::move(
                        _generate_schema<ValueType,array_annotations.size(),array_annotations>()
                    );
                }
            }else{
                static_assert(std::meta::is_class_type(^^decayed_T),"Unsupported Structure!");
            }

            if(add_constaint){
                auto & restraint = root[0].value().transform<std::string_view>();
                
                /// 这里是检测constraint
                template for(
                    constexpr auto anno 
                    : 
                    annotations
                ){
                    constexpr auto anno_type_info = std::meta::type_of(anno);
                    using AnnotationType = [: anno_type_info :];
                    using AnnotationBaseType = std::decay_t<AnnotationType>;                
                    constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];

                    if constexpr(std::is_same_v<AnnotationBaseType,attr::range>){
                        if(value_mapping.min != attr::no_range){
                            restraint += " MIN ";
                            restraint += std::to_string(value_mapping.min);
                        }
                        if(value_mapping.max != attr::no_range){
                            restraint += " MAX ";
                            restraint += std::to_string(value_mapping.max);
                        }
                    }else if constexpr(std::is_same_v<AnnotationBaseType,attr::validate>){
                        restraint += " VALIDATE ";
                        restraint += value_mapping.validate_function;
                    }else{
                        if constexpr(
                            requires{
                                AnnotationBaseType::attribute_trait;
                            }
                        ){
                            if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::ValidateWithArgs){
                                restraint += " VALIDATE ";
                                restraint += value_mapping.validate_function;

                                if constexpr(AnnotationBaseType::ArgCount){
                                    restraint += " ( ";

                                    for(size_t i = 0;i < AnnotationBaseType::ArgCount;++i){
                                        restraint += "\"";
                                        restraint += std::string_view(value_mapping.args[i]);
                                        restraint += "\" ";
                                    }

                                    restraint += " ) ";
                                }
                            }
                        }
                    }
                }
            }

            return root;
        }
    }
}

//// Implementation
namespace alib5{
    template<class InT> 
    void from_adata(InT & fill_data,const AData & root){
        detail::_from_adata(fill_data,root);
    }

    template<class InT> 
    InT from_adata(const AData & root){
        InT object;
        detail::_from_adata(object,root);
        return object;
    }

    template<class InT> 
    AData to_adata(InT && base){
        return detail::_to_adata(std::forward<InT>(base));
    }

    template<class T>
    AData generate_schema(){
        return detail::_generate_schema<T>();
    }

}
#endif