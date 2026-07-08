/**
 * @file from_adata.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Deserialization from AData support.
 * 
 * \par Original Comment
 * English: frommm
 * Chinese: frommm
 * 
 * @version 5.0
 * @date 2026-06-10
 * @copyright Copyright (c) 2026
 */
#ifndef ALIB5_ADATA_REFLECT_FROM_ADATA
#define ALIB5_ADATA_REFLECT_FROM_ADATA
#include <alib5/data/reflect/kernel.h>
#include <alib5/data/kernel.h>

namespace alib5::detail {
    
    /**
     * @brief Loads data from an AData structure into a C++ object.
     * 
     * \par Original Comment
     * English: Load from data.
     * Chinese: 从数据中加载
     * 
     * @tparam cfg Configuration struct for conversion.
     * @tparam N Number of annotations.
     * @tparam annotations Compile-time array of meta annotations.
     * @tparam LoggerType Type of the debug logger.
     * @tparam InT Type of the data object to be filled.
     * @param fill_data The data object to be populated.
     * @param root The source AData node.
     * @param debug_logger Optional logger for debugging.
     */
    template<
        FromADataConfig cfg = default_from_adata_config,
        size_t N = 0,
        std::array<std::meta::info, N> annotations = {},
        class LoggerType = std::nullptr_t,
        class InT,
        class Data
    > 
    void _from_adata(InT & fill_data, const Data & root, LoggerType * debug_logger = nullptr);

}

namespace alib5::detail {

    template<
        FromADataConfig cfg,
        size_t N,
        std::array<std::meta::info, N> annotations,
        class LoggerType,
        class InT,
        class Data
    > 
    inline void _from_adata(InT & fill_data, const Data & root, LoggerType * debug_logger) {   
        constexpr std::meta::access_context context = std::meta::access_context::unchecked(); 
        
        if constexpr(std::is_enum_v<std::decay_t<InT>>) {
           auto & mapper = get_enum_mapper<std::decay_t<InT>>();

           if(auto it = mapper.find(root.template to<std::string_view>()); it != mapper.end()) {
                fill_data = it->second;
           } else {
                // Do nothing for now. (Original: 啥都不做现在)
           }
        } else if constexpr(IsNodeValue<InT>) {
            panic_if(
                !root.is_value(),
                "Node is not a value!"
            );

            fill_data = root.template to<InT>();

            if constexpr(cfg.debug) {
                if(debug_logger) [[likely]] {
                    *debug_logger << "Type  : " << std::meta::display_string_of(^^InT) << fls;
                    *debug_logger << "Value : " << fill_data << "\n" << fls;
                }
            }
        } else if constexpr(
            requires {
                typename InT::value_type;
                typename InT::value_type();
            }
            &&
            requires(typename InT::value_type & val) {
                fill_data.clear();
                fill_data.push_back(val);
                { fill_data.back() } -> std::convertible_to<typename InT::value_type &>;
            }
        ) {
            panic_if(
                !root.is_array(),
                "Node is not an array!"
            );
            auto & arr = root.array();

            // Check if pre-reserve is supported. (Original: 是否支持提前reserve)
            if constexpr(requires{ fill_data.reserve((size_t)1); }) {
                fill_data.reserve(arr.size());
            }

            using ValueType = typename InT::value_type;

            if constexpr(cfg.debug) {
                if(debug_logger) [[likely]] {
                    *debug_logger << "Type      : " << std::meta::display_string_of(^^InT) << fls;
                    *debug_logger << "ValueType : " << std::meta::display_string_of(^^ValueType) << "\n" << fls;
                }
            }

            fill_data.clear();
            for(size_t i = 0; i < arr.size(); ++i) {
                fill_data.push_back(ValueType());

                constexpr static auto result = annotation_do_if_trait<
                    attr::AttributeTraits::InnerAttributes,
                    N,
                    annotations
                >();

                if constexpr(not_found_annotation(result)) {
                    _from_adata<cfg>(fill_data.back(), arr[i], debug_logger);
                } else {
                    constexpr static auto array_annotations = result.to_annotation_array();

                        _from_adata<
                        cfg,
                        array_annotations.size(),
                        array_annotations
                    >(fill_data.back(), arr[i], debug_logger);
                }
            }
        } else if constexpr(std::meta::is_class_type(^^InT)) {
            panic_if(
                !root.is_object(),
                "Node is not an object!"
            );

            template for(
                constexpr auto item :
                std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^InT, context)
                )
            ) {
                constexpr auto element_type_info = std::meta::type_of(item);

                if constexpr(!std::meta::is_reference_type(element_type_info)) {
                    constexpr auto original_name = std::meta::identifier_of(item);
                    std::string_view name = original_name;

                    constexpr static auto child_annotations = std::define_static_array(
                        std::meta::annotations_of(item)
                    );
                    constexpr static auto array_annotations = detail::ranges_to_array<std::meta::info, child_annotations.size()>(
                        child_annotations
                    );

                    if constexpr(
                        !has_annotation_with_trait<attr::AttributeTraits::DeseriSkip, array_annotations.size(), array_annotations>() &&
                        !has_annotation_with_trait<attr::AttributeTraits::GeneralSkip, array_annotations.size(), array_annotations>()
                    ) {
                        // Check annotation. (Original: 检测annotation)
                        constexpr static auto rename_attr = annotation_do_if_trait<attr::AttributeTraits::Rename, array_annotations.size(), array_annotations>();
                        if constexpr(!not_found_annotation(rename_attr)) name = rename_attr.new_name();

                        if constexpr(cfg.debug) {
                            if(debug_logger) [[likely]] {
                                *debug_logger << "Type         : " << std::meta::display_string_of(^^InT) << fls
                                              << "OriginalName : " << original_name << fls
                                              << "MappingName  : " << name << "\n" << fls
                                              << "ExistInData  : " << root.object().contains(name) << "\n" << fls;
                            }
                        }

                        if(root.object().contains(name)) {
                            _from_adata<
                                cfg,
                                array_annotations.size(),
                                array_annotations
                            >(fill_data.[: item :], root[name], debug_logger);
                        }
                    }
                }
            }

        } else {
#ifdef ALIB5_ENABLE_STRICT_REFLECTION
            static_assert(std::meta::is_class_type(^^InT), "Unsupported Structure!");
#else
            return;
#endif
        }
    }   
}

#endif