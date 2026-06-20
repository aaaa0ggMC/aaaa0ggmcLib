/**
 * @file to_adata.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Serialization to AData support.
 * 
 * \par Original Comment
 * English: tooooooooo
 * Chinese: tooooooooo
 * 
 * @version 5.0
 * @date 2026-06-10
 * @copyright Copyright (c) 2026
 */
#ifndef ALIB5_ADATA_REFLECT_TO_ADATA
#define ALIB5_ADATA_REFLECT_TO_ADATA
#include <alib5/data/reflect/kernel.h>

namespace alib5::detail {

    /**
     * @brief Converts a base C++ structure/value into an AData representation.
     * 
     * @tparam cfg Configuration struct for conversion.
     * @tparam N Number of annotations.
     * @tparam annotations Compile-time array of meta annotations.
     * @tparam LoggerType Type of the debug logger.
     * @tparam InT Type of the input base structure.
     * @param base The base input value to convert.
     * @param debug_logger Optional logger for debugging.
     * @return AData The resulting serialized data.
     */
    template<
        ToADataConfig cfg = default_to_adata_config,
        size_t N = 0,
        std::array<std::meta::info, N> annotations = {},
        class LoggerType = std::nullptr_t,
        class InT
    > 
    AData _to_adata(InT && base, LoggerType * debug_logger = nullptr);

}

namespace alib5::detail {

    template<
        ToADataConfig cfg,
        size_t N,
        std::array<std::meta::info, N> annotations,
        class LoggerType,
        class InT
    > 
    inline AData _to_adata(InT && base, LoggerType * debug_logger) {
        using T = std::decay_t<InT>;
        constexpr std::meta::info t_info = ^^T;
        constexpr std::meta::access_context context = std::meta::access_context::unchecked();

        AData root;

        constexpr auto target_is_direct = [&]() consteval -> bool {
            return requires {
                root = std::forward<InT>(base);
            };
        };
        constexpr auto target_is_object = [&](std::meta::info info) consteval -> bool {
            return std::meta::is_class_type(info);
        };

        if constexpr(std::is_enum_v<std::decay_t<T>>) {
            auto & mapper = get_enum_string_mapper<std::decay_t<T>>();

            if(auto it = mapper.find(base); it != mapper.end()) {
                root = it->second;
            } else {
                // Still do nothing. (Original: 依旧啥都不干)
            }
        } else if constexpr(target_is_direct()) {
            root = std::forward<InT>(base);

            if constexpr(cfg.debug) {
                if(debug_logger) [[likely]] {
                    *debug_logger << "Type  : " << std::meta::display_string_of(^^InT) << fls;
                    *debug_logger << "Value : " << base << "\n" << fls;
                }
            }
        } else if constexpr(detail::target_is_array< ^^base >()) {
            root.set<darray_t>();
            size_t size = std::forward<InT>(base).size();
            
            if(size) root.array().reserve(size);

            if constexpr(cfg.debug) {
                if(debug_logger) [[likely]] {
                    bool has_pass = false;
                    if constexpr(
                        detail::has_annotation_with_trait<attr::AttributeTraits::InnerAttributes, N, annotations>()
                    ) {
                        has_pass = true;
                    }
                    
                    *debug_logger << "Type          : " << std::meta::display_string_of(^^InT) << fls;
                    if constexpr(
                        requires {
                            typename InT::value_type;
                        }
                    ) {
                        using ValueType = typename InT::value_type;
                        *debug_logger << "ValueType     : " << std::meta::display_string_of(^^ValueType) << fls;
                    }
                    *debug_logger << "InnerAttribute : " << has_pass << "\n" << fls;
                }
            }

            for(size_t i = 0; i < size; ++i) {
                constexpr static auto result = annotation_do_if_trait<
                    attr::AttributeTraits::InnerAttributes,
                    N,
                    annotations
                >();

                if constexpr(not_found_annotation(result)) {
                    root[i] = std::move(
                        _to_adata<cfg>(
                            std::forward<InT>(base)[i], debug_logger
                        )
                    );
                } else {
                    constexpr static auto array_annotations = result.to_annotation_array();

                    root[i] = std::move(
                        _to_adata<
                            cfg,
                            array_annotations.size(),
                            array_annotations
                        >(
                            std::forward<InT>(base)[i], debug_logger
                        )
                    );
                }
            }                
        } else if constexpr(target_is_object(t_info)) {
            constexpr static auto items = 
            std::define_static_array(
                std::meta::nonstatic_data_members_of(
                    t_info,
                    context
                )
            );
            constexpr size_t item_size = items.size();

            root.set<dobject_t>();
            root.object().reserve(item_size);

            template for(constexpr auto item : items) {
                // Check if it is static. (Original: 检查是不是static的)
                constexpr auto item_type = std::meta::type_of(item);

                if constexpr(!std::meta::is_reference_type(item_type)) {
                    constexpr auto original_name = std::meta::identifier_of(item);

                    std::string_view name = original_name;

                    constexpr static auto child_annotations = std::define_static_array(
                        std::meta::annotations_of(item)
                    );
                    constexpr static auto array_annotations = detail::ranges_to_array<std::meta::info, child_annotations.size()>(
                        child_annotations
                    );

                    if constexpr(
                        !has_annotation_with_trait<attr::AttributeTraits::SeriSkip, array_annotations.size(), array_annotations>() &&
                        !has_annotation_with_trait<attr::AttributeTraits::GeneralSkip, array_annotations.size(), array_annotations>()
                    ) {
                        // Check annotation. (Original: 检测annotation)
                        constexpr static auto rename_attr = annotation_do_if_trait<attr::AttributeTraits::Rename, array_annotations.size(), array_annotations>();
                        if constexpr(!not_found_annotation(rename_attr)) name = rename_attr.new_name();

                        constexpr bool need_omit = has_annotation_with_trait<attr::AttributeTraits::OmitEmpty, array_annotations.size(), array_annotations>();

                        // Cast first if possible. (Original: 如果能cast就先cast)
                        if constexpr(requires{ root[name] = std::forward<InT>(base).[: item :]; }) {
                            auto & node = root[name];
                            node = std::forward<InT>(base).[: item :];               
                            
                            if constexpr(cfg.debug) {
                                if(debug_logger) [[likely]] {
                                    *debug_logger << "Type         : " << std::meta::display_string_of(^^InT) << fls
                                                  << "OriginalName : " << original_name << fls
                                                  << "MappingName  : " << name << fls
                                                  << "Value        : " << node.to<std::string_view>() << fls
                                                  << "OmitEmpty    : " << need_omit << fls;
                                }
                            }

                            bool omitted = false;
                            if constexpr(need_omit) {
                                // Because I don't know what it is after the data is formatted... Although it is basic data, extra steps are still needed.
                                // Original: 因为我不知道数据格式化进去后是啥玩意儿。。。。 虽然是基础数据，但是还是需要额外的步骤
                                if(node.is_value() && node.value().get_type() == Value::Type::STRING && node.to<std::string_view>() == "") {
                                    root.object().remove(name);
                                    omitted = true;
                                }
                            }

                            if constexpr(cfg.debug) {
                                if(debug_logger) [[likely]] *debug_logger << "Omitted      : " << omitted << "\n" << fls;
                            }
                        } else {
                            bool cond = true;
                            using Type = decltype(base.[: item :]);
                            Type target = std::forward<Type>(base.[: item :]);

                            if constexpr(detail::target_is_array< ^^target >() && need_omit) {
                                if(!std::forward<InT>(base).[: item :].size()) cond = false;
                            }

                            if constexpr(cfg.debug) {
                                if(debug_logger) [[likely]] {
                                    *debug_logger << "Type         : " << std::meta::display_string_of(^^InT) << fls
                                                  << "OriginalName : " << original_name << fls
                                                  << "MappingName  : " << name << fls
                                                  << "OmitEmpty    : " << need_omit << fls;
                                }
                            }

                            if(!cond) {
                                root[name] = std::move(_to_adata<cfg, child_annotations.size(), array_annotations>(std::forward<InT>(base).[: item :], debug_logger));
                                if constexpr(cfg.debug) if(debug_logger) [[likely]] *debug_logger << "Omitted      : " << true << "\n" << fls;
                            } else {
                                if constexpr(cfg.debug) if(debug_logger) [[likely]] *debug_logger << "Omitted      : " << false << "\n" << fls;
                            }
                        }
                    }
                }
            }
        } else {
#ifdef ALIB5_ENABLE_STRICT_REFLECTION
            static_assert(
                std::meta::is_class_type(t_info),
                "Unsupported Structure!"
            );
#else
            return root;
#endif
        }

        return root;
    }

}
#endif