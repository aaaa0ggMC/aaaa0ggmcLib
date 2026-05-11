#ifndef ALIB5_ADATA_REFLECT_FROM_ADATA
#define ALIB5_ADATA_REFLECT_FROM_ADATA
#include <alib5/data/reflect/kernel.h>
#include <alib5/data/kernel.h>

namespace alib5::detail {
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

                constexpr static auto result = annotation_do_if_trait<
                    attr::AttributeTraits::InnerAttributes,
                    N,
                    annotations
                >();

                if constexpr(
                    not_found_annotation(result)
                ){
                    _from_adata<cfg>(fill_data.back(),arr[i],debug_logger);
                }else{
                    constexpr static auto array_annotations = result.to_annotation_array();

                        _from_adata<
                        cfg,
                        array_annotations.size(),
                        array_annotations
                    >(fill_data.back(),arr[i],debug_logger);
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
}

#endif