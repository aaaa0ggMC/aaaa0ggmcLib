#ifndef ALIB5_ADATA_REFLECT_TO_ADATA
#define ALIB5_ADATA_REFLECT_TO_ADATA
#include <alib5/data/reflect/kernel.h>

namespace alib5::detail {
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
                        detail::has_annotation_with_trait<attr::AttributeTraits::InnerAttributes,N,annotations>()
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
                    *debug_logger << "InnerAttribute : " << has_pass << "\n" << fls;
                }
            }

            for(size_t i = 0;i < size;++i){
                constexpr static auto result = annotation_do_if_trait<
                    attr::AttributeTraits::InnerAttributes,
                    N,
                    annotations
                >();

                if constexpr(
                    not_found_annotation(result)
                ){
                    root[i] = std::move(
                        _to_adata<cfg>(
                            std::forward<InT>(base)[i],debug_logger
                        )
                    );
                }else{
                    constexpr static auto array_annotations = result.to_annotation_array();

                    root[i] = std::move(
                        _to_adata<
                            cfg,
                            array_annotations.size(),
                            array_annotations
                        >(
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
                    
                    if constexpr(requires{
                        AnnotationBaseType::attribute_trait;
                    }){
                        constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];
                       
                        if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::Rename){
                            if(value_mapping.new_name != nullptr){
                                name = value_mapping.new_name;
                            }
                        }
                    }
                }

                constexpr bool need_omit = 
                    has_annotation_with_trait<
                        attr::AttributeTraits::OmitEmpty,
                        array_annotations.size(),
                        array_annotations
                    >();

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
}
#endif