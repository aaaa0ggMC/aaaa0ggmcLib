#ifndef ALIB5_ADATA_REFLECT_GEN_SCHEMA
#define ALIB5_ADATA_REFLECT_GEN_SCHEMA
#include <alib5/data/reflect/kernel.h>
#include <alib5/data/reflect/to_adata.h>

namespace alib5::detail {
    template<class T,size_t N = 0,std::array<std::meta::info,N> annotations = {}>
    AData _generate_schema(){
        using decayed_T = std::decay_t<T>;
        constexpr auto context = std::meta::access_context::unchecked(); 

        AData root;

        bool add_constaint = true;

        if constexpr(
            std::is_enum_v<std::decay_t<T>>
        ){
            root[0] = "TYPE STRING ENUM ( ";

            auto & val = root[0].value().transform<std::string_view>();
            template for(
                constexpr auto item :
                std::define_static_array(std::meta::enumerators_of(
                    ^^std::decay_t<T>
                ))
            ){
                val += "\"";
                val += std::meta::identifier_of(item);
                val += "\" ";
            }

            val += ") ";
        }else if constexpr(
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
            constexpr static auto result = annotation_do_if_trait<
                attr::AttributeTraits::InnerAttributes,
                N,
                annotations
            >();

            if constexpr(
                not_found_annotation(result)
            ){
                root[1] = std::move(
                    _generate_schema<typename decayed_T::value_type>()
                );
            }else{
                constexpr static auto array_annotations = result.to_annotation_array();

                root[1] = std::move(
                    _generate_schema<
                        typename decayed_T::value_type,
                        array_annotations.size(),
                        array_annotations
                    >()
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

                constexpr auto element_type_info = std::meta::type_of(item);

                if constexpr(!std::meta::is_reference_type(element_type_info)){
                    constexpr auto original_name = std::meta::identifier_of(item);
                    
                    std::string_view name = original_name;
                    using ValueType = [: std::meta::type_of(item) :];

                    constexpr static auto child_annotations = std::define_static_array(
                        std::meta::annotations_of(item)
                    );
                    constexpr static auto array_annotations = detail::ranges_to_array<std::meta::info,child_annotations.size()>(
                        child_annotations
                    );

                    // 对于需要skip的直接进行skip
                    if constexpr(
                        !has_annotation_with_trait<attr::AttributeTraits::SchemaSkip,array_annotations.size(),array_annotations>() &&
                        !has_annotation_with_trait<attr::AttributeTraits::GeneralSkip,array_annotations.size(),array_annotations>()
                    ){
                        // 检测annotation
                        constexpr static auto rename_attr = annotation_do_if_trait<attr::AttributeTraits::Rename, array_annotations.size(), array_annotations>();
                        if constexpr(!not_found_annotation(rename_attr)) name = rename_attr.new_name();

                        root[name] = std::move(
                            _generate_schema<ValueType,array_annotations.size(),array_annotations>()
                        );
                    }
                }
            }
        }else{
#ifdef ALIB5_ENABLE_STRICT_REFLECTION
            static_assert(std::meta::is_class_type(^^decayed_T),"Unsupported Structure!");
#else
            return root;
#endif
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

                if constexpr(requires{
                    AnnotationBaseType::attribute_trait;
                }){
                    constexpr static auto value_mapping = [: std::meta::constant_of(anno) :];
                    
                    if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::SchemaRange){
                        if(value_mapping.min != attr::no_range){
                            restraint += " MIN ";
                            restraint += std::to_string(value_mapping.min);
                        }
                        if(value_mapping.max != attr::no_range){
                            restraint += " MAX ";
                            restraint += std::to_string(value_mapping.max);
                        }
                    }else if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::SchemaValidate){
                        restraint += " VALIDATE ";
                        restraint += value_mapping.validate_function();
                    }if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::ValidateWithArgs){
                        restraint += " VALIDATE ";
                        restraint += AnnotationBaseType::validate_function();

                        if constexpr(AnnotationBaseType::ArgCount){
                            restraint += " ( ";

                            auto append = [&restraint]<detail::fixed_string & S>{
                                restraint += "\"";
                                restraint += S.view();
                                restraint += "\"";
                            };

                            [&append]<std::size_t... Is>(std::index_sequence<Is...>){
                                (... , append.template operator()<
                                    AnnotationBaseType::template get_arg<Is>()
                                >() );
                            }(std::make_index_sequence<AnnotationBaseType::ArgCount>{});

                            restraint += " ) ";
                        }
                    }else if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::DefaultValue){
                        root[1] = std::move(
                            _to_adata(value_mapping.value)
                        );
                    }else if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::OverrideIfConflict){
                        restraint += " OVERRIDE_CONFLICT";
                    }else if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::Optional){
                        restraint += " OPTIONAL";
                    }else if constexpr(AnnotationBaseType::attribute_trait == attr::AttributeTraits::Extra){
                        restraint += " ";
                        restraint += AnnotationBaseType::extra_restraints();
                    }
                }
            }
        }

        return root;
    }
}

#endif