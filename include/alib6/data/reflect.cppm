module;
#include <meta>
#include <limits>
#include <string_view>
#include <optional>
#include <concepts>
#include <type_traits>
#include <utility>
#include <array>
#include <tuple>
#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <print>

export module alib6.data:reflect;

import alib6.core;
import :concepts;
import :kernel;
import :validator;

export namespace alib6::attr {

    /**
     * @brief 编译期固定长度字符串，用于模板元编程中的注解参数
     */
    template<std::size_t N>
    struct fixed_string {
        char data[N]{};

        constexpr fixed_string(const char (&str)[N]) noexcept {
            for (std::size_t i = 0; i < N; ++i) {
                data[i] = str[i];
            }
        }

        [[nodiscard]] constexpr std::string_view view() const noexcept {
            return std::string_view{data, N > 0 ? N - 1 : 0};
        }

        [[nodiscard]] constexpr const char* c_str() const noexcept { return data; }
    };

    template<std::size_t N>
    fixed_string(const char (&)[N]) -> fixed_string<N>;

    inline constexpr double no_range = std::numeric_limits<double>::infinity();

    enum class Trait {
        Rename,
        Alias,
        Skip,
        SeriSkip,
        SeriOmitEmpty,
        DeseriSkip,
        SchemaSkip,
        FillBy,
        SchemaOptional,
        SchemaRange,
        SchemaValidate,
        SchemaValidateArgs,
        SchemaDefaultValue,
        SchemaFallback,
        SchemaExtra,
        InnerAttributes,
        MappingDict,
    };

    // ==========================================
    // 1. 通用注解 (Serialization, Deserialization, Schema)
    // ==========================================

    template<fixed_string S>
    struct rename {
        static constexpr Trait trait = Trait::Rename;
        static constexpr std::string_view name() noexcept { return S.view(); }
    };

    template<fixed_string... Aliases>
    struct alias {
        static constexpr Trait trait = Trait::Alias;
        static constexpr std::size_t count = sizeof...(Aliases);

        static constexpr bool contains(std::string_view name) noexcept {
            bool matched = false;
            ([&] {
                if (Aliases.view() == name) matched = true;
            }(), ...);
            return matched;
        }
    };

    template<fixed_string S>
    struct fill_by {
        static constexpr Trait trait = Trait::FillBy;
        static constexpr std::string_view source_name() noexcept { return S.view(); }
    };

    template<fixed_string S>
    using fill_from = fill_by<S>;

    struct skip {
        static constexpr Trait trait = Trait::Skip;
    };

    // ==========================================
    // 2. 序列化专用注解
    // ==========================================
    namespace seri {
        struct skip { static constexpr Trait trait = Trait::SeriSkip; };
        struct omit_empty { static constexpr Trait trait = Trait::SeriOmitEmpty; };
    }

    // ==========================================
    // 3. 反序列化专用注解
    // ==========================================
    namespace deseri {
        struct skip { static constexpr Trait trait = Trait::DeseriSkip; };
    }

    // ==========================================
    // 4. Schema 生成与校验专用注解
    // ==========================================
    namespace schema {
        struct skip { static constexpr Trait trait = Trait::SchemaSkip; };
        struct optional { static constexpr Trait trait = Trait::SchemaOptional; };
        struct fallback { static constexpr Trait trait = Trait::SchemaFallback; }; // OVERRIDE_CONFLICT

        struct range {
            static constexpr Trait trait = Trait::SchemaRange;
            double min{no_range};
            double max{no_range};
        };

        template<fixed_string FName>
        struct validate {
            static constexpr Trait trait = Trait::SchemaValidate;
            static constexpr std::string_view func() noexcept { return FName.view(); }
        };

        template<fixed_string FName, fixed_string... Args>
        struct validate_args {
            static constexpr Trait trait = Trait::SchemaValidateArgs;
            static constexpr std::string_view func() noexcept { return FName.view(); }
            static constexpr std::size_t arg_count = sizeof...(Args);

            template<std::size_t Idx>
            static constexpr auto& get_arg() {
                static constexpr auto t = std::make_tuple(&Args...);
                return *std::get<Idx>(t);
            }
        };

        template<class T>
        struct default_value {
            static constexpr Trait trait = Trait::SchemaDefaultValue;
            T& value;
        };

        template<fixed_string ExtraStr>
        struct extra {
            static constexpr Trait trait = Trait::SchemaExtra;
            static constexpr std::string_view rules() noexcept { return ExtraStr.view(); }
        };

        template<fixed_string DictRule = "">
        struct mapping {
            static constexpr Trait trait = Trait::MappingDict;
            static constexpr std::string_view rule() noexcept { return DictRule.view(); }
        };
    }

    // ==========================================
    // 5. 容器内部元素注解穿透
    // ==========================================
    template<class... Ts>
    struct element_attr {
        static constexpr Trait trait = Trait::InnerAttributes;
        std::tuple<Ts...> items;
        constexpr element_attr(Ts... args) : items(args...) {}
    };

} // namespace alib6::attr

namespace alib6::detail::refl {

    template<class T>
    concept IsOptional = requires(T t) {
        typename T::value_type;
        { t.has_value() } -> std::convertible_to<bool>;
        { *t } -> std::same_as<typename T::value_type&>;
    };

    template<class T>
    concept IsMapLike = requires(T t) {
        typename T::key_type;
        typename T::mapped_type;
        { t.begin() } -> std::input_or_output_iterator;
        { t.end() } -> std::input_or_output_iterator;
        requires std::convertible_to<typename T::key_type, std::string_view>;
    };

    template<class T>
    concept IsSequenceContainer = !IsMapLike<T> && !std::same_as<std::decay_t<T>, std::string> &&
                                  !std::same_as<std::decay_t<T>, pmr::string> &&
                                  !std::same_as<std::decay_t<T>, std::string_view> &&
                                  requires(T t) {
        typename T::value_type;
        { t.size() } -> std::convertible_to<std::size_t>;
        { t.begin() } -> std::input_or_output_iterator;
        { t.end() } -> std::input_or_output_iterator;
    };

    // 枚举字符串双向映射工具
    template<class E>
        requires std::is_enum_v<std::decay_t<E>>
    struct EnumMapper {
        using EnumType = std::decay_t<E>;

        static std::string_view to_string(EnumType val) noexcept {
            static constexpr auto items = std::define_static_array(std::meta::enumerators_of(^^EnumType));
            template for (constexpr auto item : items) {
                if ([: item :] == val) {
                    return std::meta::identifier_of(item);
                }
            }
            return "";
        }

        static std::optional<EnumType> from_string(std::string_view str) noexcept {
            static constexpr auto items = std::define_static_array(std::meta::enumerators_of(^^EnumType));
            template for (constexpr auto item : items) {
                if (std::meta::identifier_of(item) == str) {
                    return [: item :];
                }
            }
            return std::nullopt;
        }
    };

    // 注解提取辅助函数
    template<std::meta::info Member>
    struct MemberAnnotationInspector {
        template<attr::Trait Tr>
        static constexpr bool has_trait() noexcept {
            static constexpr auto annos = std::define_static_array(std::meta::annotations_of(Member));
            template for (constexpr auto anno : annos) {
                constexpr auto anno_type = std::meta::type_of(anno);
                using AnnoT = [: anno_type :];
                using Decayed = std::decay_t<AnnoT>;
                if constexpr (requires { Decayed::trait; }) {
                    if constexpr (Decayed::trait == Tr) {
                        return true;
                    }
                }
            }
            return false;
        }

        static constexpr std::string_view get_name() noexcept {
            static constexpr auto annos = std::define_static_array(std::meta::annotations_of(Member));
            template for (constexpr auto anno : annos) {
                constexpr auto anno_type = std::meta::type_of(anno);
                using AnnoT = [: anno_type :];
                using Decayed = std::decay_t<AnnoT>;
                if constexpr (requires { Decayed::trait; }) {
                    if constexpr (Decayed::trait == attr::Trait::Rename) {
                        return AnnoT::name();
                    }
                }
            }
            return std::meta::identifier_of(Member);
        }

        static constexpr bool has_fill_by() noexcept {
            return has_trait<attr::Trait::FillBy>();
        }

        static constexpr std::string_view get_fill_by_source() noexcept {
            static constexpr auto annos = std::define_static_array(std::meta::annotations_of(Member));
            template for (constexpr auto anno : annos) {
                constexpr auto anno_type = std::meta::type_of(anno);
                using AnnoT = [: anno_type :];
                using Decayed = std::decay_t<AnnoT>;
                if constexpr (requires { Decayed::trait; }) {
                    if constexpr (Decayed::trait == attr::Trait::FillBy) {
                        return AnnoT::source_name();
                    }
                }
            }
            return "";
        }

        static constexpr bool is_seri_skip() noexcept {
            return has_trait<attr::Trait::Skip>() || has_trait<attr::Trait::SeriSkip>() || has_fill_by();
        }

        static constexpr bool is_deseri_skip() noexcept {
            return has_trait<attr::Trait::Skip>() || has_trait<attr::Trait::DeseriSkip>() || has_fill_by();
        }

        static constexpr bool is_schema_skip() noexcept {
            return has_trait<attr::Trait::Skip>() || has_trait<attr::Trait::SchemaSkip>() || has_fill_by();
        }

        static constexpr bool is_omit_empty() noexcept {
            return has_trait<attr::Trait::SeriOmitEmpty>();
        }

        static constexpr bool has_rename() noexcept {
            return has_trait<attr::Trait::Rename>();
        }

        static constexpr bool matches_alias(std::string_view candidate) noexcept {
            if (candidate.empty()) return false;
            static constexpr auto annos = std::define_static_array(std::meta::annotations_of(Member));
            template for (constexpr auto anno : annos) {
                constexpr auto anno_type = std::meta::type_of(anno);
                using AnnoT = [: anno_type :];
                using Decayed = std::decay_t<AnnoT>;
                if constexpr (requires { Decayed::trait; }) {
                    if constexpr (Decayed::trait == attr::Trait::Alias) {
                        if (AnnoT::contains(candidate)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        static constexpr bool matches_name(std::string_view candidate) noexcept {
            if (candidate == get_name() || candidate == std::meta::identifier_of(Member)) {
                return true;
            }
            return matches_alias(candidate);
        }

        /**
         * @brief 计算当前目标成员与源成员的匹配精准度得分 (Score)
         * 优先级: Alias (3) > Rename (2) > Origin (1) > No match (0)
         */
        template<std::meta::info OtherMember>
        static consteval int match_score() noexcept {
            using OtherInspector = MemberAnnotationInspector<OtherMember>;

            constexpr auto tgt_origin = std::meta::identifier_of(Member);
            constexpr auto tgt_rename = has_rename() ? get_name() : std::string_view{};
            constexpr auto src_origin = std::meta::identifier_of(OtherMember);
            constexpr auto src_rename = OtherInspector::has_rename() ? OtherInspector::get_name() : std::string_view{};

            // 1. Alias 匹配 (优先级最高: Score 3)
            if (OtherInspector::matches_alias(tgt_origin) || (!tgt_rename.empty() && OtherInspector::matches_alias(tgt_rename))) {
                return 3;
            }
            if (matches_alias(src_origin) || (!src_rename.empty() && matches_alias(src_rename))) {
                return 3;
            }

            // 2. Rename 匹配 (优先级第二: Score 2)
            if (!tgt_rename.empty() && !src_rename.empty() && tgt_rename == src_rename) {
                return 2;
            }
            if (!src_rename.empty() && src_rename == tgt_origin) {
                return 2;
            }
            if (!tgt_rename.empty() && tgt_rename == src_origin) {
                return 2;
            }

            // 3. Origin 标识符原始名称匹配 (优先级基础: Score 1)
            if (tgt_origin == src_origin) {
                return 1;
            }

            return 0;
        }

        template<std::meta::info OtherMember>
        static consteval bool matches_member() noexcept {
            return match_score<OtherMember>() > 0;
        }
    };

} // namespace alib6::detail::refl

export namespace alib6 {
    using Value = alib6::data::Value;
    using AData = alib6::data::AData;
    template<class V = alib6::data::Value>
    using BasicAData = alib6::data::BasicAData<V>;

    // ========================================================================
    // 1. to_adata: C++ 数据结构序列化为 AData
    // ========================================================================

    template<class T>
    AData to_adata(const T& base, memory_resource* mem = get_default_resource(), ErrorWrapper err = {}) {
        using DecayedT = std::decay_t<T>;
        AData root(mem);

        if constexpr (std::same_as<DecayedT, AData>) {
            root = base;
        } else if constexpr (std::is_enum_v<DecayedT>) {
            auto str = detail::refl::EnumMapper<DecayedT>::to_string(base);
            if (str.empty()) {
                err.report("Unmapped enum value during to_adata", CodeWithLocation(400));
                root = static_cast<int64_t>(base);
            } else {
                root = str;
            }
        } else if constexpr (alib6::data::IsNodeValue<DecayedT>) {
            root = base;
        } else if constexpr (detail::refl::IsOptional<DecayedT>) {
            if (base.has_value()) {
                root = to_adata(*base, mem, err);
            } else {
                root.set_null();
            }
        } else if constexpr (detail::refl::IsMapLike<DecayedT>) {
            root._set_object();
            for (const auto& [k, v] : base) {
                root[std::string_view(k)] = to_adata(v, mem, err);
            }
        } else if constexpr (detail::refl::IsSequenceContainer<DecayedT>) {
            root._set_array();
            auto& arr = root.array();
            arr.reserve(base.size());
            std::size_t idx = 0;
            for (const auto& item : base) {
                root[static_cast<std::ptrdiff_t>(idx++)] = to_adata(item, mem, err);
            }
        } else if constexpr (std::meta::is_class_type(^^DecayedT)) {
            constexpr auto context = std::meta::access_context::unchecked();
            constexpr auto type_info = std::meta::dealias(^^DecayedT);
            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(type_info, context)
            );

            root._set_object();

            template for (constexpr auto member : members) {
                constexpr auto member_type = std::meta::type_of(member);
                if constexpr (!std::meta::is_reference_type(member_type)) {
                    using Inspector = detail::refl::MemberAnnotationInspector<member>;
                    if constexpr (!Inspector::is_seri_skip()) {
                        constexpr auto name = Inspector::get_name();
                        const auto& val = base.[: member :];

                        bool omit = false;
                        if constexpr (Inspector::is_omit_empty()) {
                            if constexpr (detail::refl::IsOptional<std::decay_t<decltype(val)>>) {
                                if (!val.has_value()) omit = true;
                            } else if constexpr (std::convertible_to<decltype(val), std::string_view>) {
                                if (std::string_view(val).empty()) omit = true;
                            } else if constexpr (requires { val.empty(); }) {
                                if (val.empty()) omit = true;
                            }
                        }

                        if (!omit) {
                            root[name] = to_adata(val, mem, err);
                        }
                    }
                }
            }
        } else {
            err.report("Unsupported type in to_adata", CodeWithLocation(400));
        }

        return root;
    }

    // ========================================================================
    // 2. fill_matching: 异类结构体静态反射自动对齐填充
    // ========================================================================

    template<class Target, class Source>
    constexpr void fill_matching(Target& target, const Source& source) {
        using DecayedT = std::decay_t<Target>;
        using DecayedS = std::decay_t<Source>;

        if constexpr (std::meta::is_class_type(^^DecayedT) && std::meta::is_class_type(^^DecayedS)) {
            constexpr auto context = std::meta::access_context::unchecked();
            constexpr auto tgt_info = std::meta::dealias(^^DecayedT);
            constexpr auto src_info = std::meta::dealias(^^DecayedS);

            static constexpr auto tgt_members = std::define_static_array(
                std::meta::nonstatic_data_members_of(tgt_info, context)
            );
            static constexpr auto src_members = std::define_static_array(
                std::meta::nonstatic_data_members_of(src_info, context)
            );

            template for (constexpr auto m_tgt : tgt_members) {
                using TgtInspector = detail::refl::MemberAnnotationInspector<m_tgt>;
                if constexpr (!TgtInspector::is_deseri_skip()) {
                    constexpr int max_score = []() consteval {
                        int highest = 0;
                        template for (constexpr auto m_src : src_members) {
                            using SrcInspector = detail::refl::MemberAnnotationInspector<m_src>;
                            if constexpr (!SrcInspector::is_seri_skip()) {
                                using TgtT = [: std::meta::type_of(m_tgt) :];
                                using SrcT = [: std::meta::type_of(m_src) :];
                                if constexpr (std::is_assignable_v<TgtT&, const SrcT&> ||
                                              std::is_constructible_v<std::decay_t<TgtT>, const SrcT&>) {
                                    constexpr int sc = TgtInspector::template match_score<m_src>();
                                    if (sc > highest) highest = sc;
                                }
                            }
                        }
                        return highest;
                    }();

                    if constexpr (max_score > 0) {
                        bool assigned = false;
                        template for (constexpr auto m_src : src_members) {
                            using SrcInspector = detail::refl::MemberAnnotationInspector<m_src>;
                            if constexpr (!SrcInspector::is_seri_skip()) {
                                using TgtT = [: std::meta::type_of(m_tgt) :];
                                using SrcT = [: std::meta::type_of(m_src) :];
                                if constexpr (std::is_assignable_v<TgtT&, const SrcT&> ||
                                              std::is_constructible_v<std::decay_t<TgtT>, const SrcT&>) {
                                    constexpr int sc = TgtInspector::template match_score<m_src>();
                                    if (sc == max_score && !assigned) {
                                        if constexpr (std::is_assignable_v<TgtT&, const SrcT&>) {
                                            target.[: m_tgt :] = source.[: m_src :];
                                            assigned = true;
                                        } else if constexpr (std::is_constructible_v<std::decay_t<TgtT>, const SrcT&>) {
                                            target.[: m_tgt :] = std::decay_t<TgtT>(source.[: m_src :]);
                                            assigned = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // 3. from_adata: AData 反序列化至 C++ 数据结构
    // ========================================================================

    template<class T>
    bool from_adata(T& out, const AData& data, ErrorWrapper err = {}) {
        using DecayedT = std::decay_t<T>;

        if constexpr (std::same_as<DecayedT, AData>) {
            out = data;
            return true;
        } else if constexpr (std::is_enum_v<DecayedT>) {
            if (data.is_value()) {
                if (data.value().get_type() == Value::Type::STRING) {
                    auto mapped = detail::refl::EnumMapper<DecayedT>::from_string(data.to<std::string_view>());
                    if (mapped) {
                        out = *mapped;
                        return true;
                    }
                } else if (data.value().get_type() == Value::Type::INT) {
                    out = static_cast<DecayedT>(data.to<int64_t>());
                    return true;
                }
            }
            err.report("Invalid enum representation in AData", CodeWithLocation(400));
            return false;
        } else if constexpr (alib6::data::IsNodeValue<DecayedT>) {
            if (!data.is_value()) {
                err.report("Expected value node for basic type in from_adata", CodeWithLocation(400));
                return false;
            }
            out = data.to<DecayedT>(err);
            return true;
        } else if constexpr (detail::refl::IsOptional<DecayedT>) {
            if (data.is_null()) {
                out = std::nullopt;
                return true;
            }
            typename DecayedT::value_type inner_val{};
            if (from_adata(inner_val, data, err)) {
                out = std::move(inner_val);
                return true;
            }
            return false;
        } else if constexpr (detail::refl::IsMapLike<DecayedT>) {
            if (!data.is_object()) {
                err.report("Expected object node for Map in from_adata", CodeWithLocation(400));
                return false;
            }
            out.clear();
            for (auto proxy : data.object()) {
                typename DecayedT::mapped_type elem{};
                if (!from_adata(elem, proxy.second(), err)) return false;
                if constexpr (requires { out.emplace(typename DecayedT::key_type(proxy.first(), out.get_allocator()), std::move(elem)); }) {
                    out.emplace(typename DecayedT::key_type(proxy.first(), out.get_allocator()), std::move(elem));
                } else {
                    out.emplace(typename DecayedT::key_type(proxy.first()), std::move(elem));
                }
            }
            return true;
        } else if constexpr (detail::refl::IsSequenceContainer<DecayedT>) {
            if (!data.is_array()) {
                err.report("Expected array node for SequenceContainer in from_adata", CodeWithLocation(400));
                return false;
            }
            const auto& arr = data.array();
            if constexpr (requires { out.clear(); out.reserve(arr.size()); }) {
                out.clear();
                out.reserve(arr.size());
            } else if constexpr (requires { out.clear(); }) {
                out.clear();
            }

            for (std::size_t i = 0; i < arr.size(); ++i) {
                typename DecayedT::value_type elem{};
                if (!from_adata(elem, arr[static_cast<std::ptrdiff_t>(i)], err)) return false;
                if constexpr (requires { out.push_back(std::move(elem)); }) {
                    out.push_back(std::move(elem));
                } else if constexpr (requires { out[i] = std::move(elem); }) {
                    if (i < out.size()) out[i] = std::move(elem);
                }
            }
            return true;
        } else if constexpr (std::meta::is_class_type(^^DecayedT)) {
            if (!data.is_object()) {
                err.report("Expected object node for struct in from_adata", CodeWithLocation(400));
                return false;
            }

            constexpr auto context = std::meta::access_context::unchecked();
            constexpr auto type_info = std::meta::dealias(^^DecayedT);
            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(type_info, context)
            );

            template for (constexpr auto member : members) {
                constexpr auto member_type = std::meta::type_of(member);
                if constexpr (!std::meta::is_reference_type(member_type)) {
                    using Inspector = detail::refl::MemberAnnotationInspector<member>;
                    if constexpr (!Inspector::is_deseri_skip()) {
                        constexpr auto name = Inspector::get_name();
                        if (data.object().contains(name)) {
                            if (!from_adata(out.[: member :], data[name], err)) {
                                return false;
                            }
                        } else {
                            for (auto proxy : data.object()) {
                                if (Inspector::matches_name(proxy.first())) {
                                    if (!from_adata(out.[: member :], proxy.second(), err)) {
                                        return false;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // 后置阶段：执行 fill_by 异类填充
            template for (constexpr auto m_tgt : members) {
                using TgtInspector = detail::refl::MemberAnnotationInspector<m_tgt>;
                if constexpr (TgtInspector::has_fill_by()) {
                    constexpr auto src_name = TgtInspector::get_fill_by_source();
                    template for (constexpr auto m_src : members) {
                        using SrcInspector = detail::refl::MemberAnnotationInspector<m_src>;
                        if constexpr (std::meta::identifier_of(m_src) == src_name || SrcInspector::get_name() == src_name) {
                            fill_matching(out.[: m_tgt :], out.[: m_src :]);
                        }
                    }
                }
            }

            return true;
        } else {
            err.report("Unsupported type in from_adata", CodeWithLocation(400));
            return false;
        }
    }

    template<class T>
    T from_adata(const AData& data, memory_resource* mem = get_default_resource(), ErrorWrapper err = {}) {
        using DecayedT = std::decay_t<T>;
        if constexpr (requires { DecayedT(mem); }) {
            DecayedT res(mem);
            from_adata(res, data, err);
            return res;
        } else {
            DecayedT res{};
            from_adata(res, data, err);
            return res;
        }
    }

    template<class... Targets>
    bool extract_from(const AData& data, Targets&... targets) {
        return (from_adata(targets, data) && ...);
    }

    // ========================================================================
    // 3. generate_schema: 基于静态类型生成 Validator 校验规则树
    // ========================================================================

    template<class T>
    AData generate_schema(memory_resource* mem = get_default_resource()) {
        using DecayedT = std::decay_t<T>;
        AData root(mem);

        auto append_to_rule = [&](AData& node, std::string_view extra) {
            if (node.is_array() && node.array().size() > 0) {
                pmr::string cur(node[0].to<std::string_view>(), mem);
                cur += extra;
                node[0] = std::move(cur);
            }
        };

        if constexpr (std::is_enum_v<DecayedT>) {
            pmr::string rule("TYPE STRING ENUM ( ", mem);
            static constexpr auto enum_items = std::define_static_array(std::meta::enumerators_of(^^DecayedT));
            template for (constexpr auto item : enum_items) {
                rule += "\"";
                rule += std::meta::identifier_of(item);
                rule += "\" ";
            }
            rule += ")";
            root[0] = std::move(rule);
        } else if constexpr (std::same_as<DecayedT, bool>) {
            root[0] = "TYPE BOOL";
        } else if constexpr (std::integral<DecayedT>) {
            root[0] = "TYPE INT";
        } else if constexpr (std::floating_point<DecayedT>) {
            root[0] = "TYPE DOUBLE";
        } else if constexpr (std::convertible_to<DecayedT, std::string_view>) {
            root[0] = "TYPE STRING";
        } else if constexpr (detail::refl::IsOptional<DecayedT>) {
            root = generate_schema<typename DecayedT::value_type>(mem);
            append_to_rule(root, " OPTIONAL");
        } else if constexpr (detail::refl::IsMapLike<DecayedT>) {
            root._set_object();
            // Object-level magic key [ALIB6_OBJ] 承接字典/映射约束
            root[alib6::data::magic_key_for_schema_restr] = "TYPE OBJECT";
        } else if constexpr (detail::refl::IsSequenceContainer<DecayedT>) {
            root[0] = "TYPE ARRAY";
            root[1] = generate_schema<typename DecayedT::value_type>(mem);
        } else if constexpr (std::meta::is_class_type(^^DecayedT)) {
            constexpr auto context = std::meta::access_context::unchecked();
            constexpr auto type_info = std::meta::dealias(^^DecayedT);
            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(type_info, context)
            );

            root._set_object();

            static constexpr auto class_annos = std::define_static_array(std::meta::annotations_of(type_info));
            pmr::string class_restr("TYPE OBJECT", mem);
            bool has_class_restr = false;

            template for (constexpr auto anno : class_annos) {
                constexpr auto anno_t = std::meta::type_of(anno);
                using AnnoT = [: anno_t :];
                using DecayedAnno = std::decay_t<AnnoT>;
                if constexpr (requires { DecayedAnno::trait; }) {
                    if constexpr (DecayedAnno::trait == attr::Trait::SchemaOptional) {
                        class_restr += " OPTIONAL";
                        has_class_restr = true;
                    } else if constexpr (DecayedAnno::trait == attr::Trait::SchemaFallback) {
                        class_restr += " OVERRIDE_CONFLICT";
                        has_class_restr = true;
                    } else if constexpr (DecayedAnno::trait == attr::Trait::SchemaExtra) {
                        class_restr += " ";
                        class_restr += AnnoT::rules();
                        has_class_restr = true;
                    } else if constexpr (DecayedAnno::trait == attr::Trait::MappingDict) {
                        if (!AnnoT::rule().empty()) {
                            class_restr += " ";
                            class_restr += AnnoT::rule();
                        }
                        has_class_restr = true;
                    }
                }
            }

            if (has_class_restr) {
                root[alib6::data::magic_key_for_schema_restr] = std::move(class_restr);
            }

            // 处理所有非静态数据成员
            template for (constexpr auto member : members) {
                constexpr auto member_type = std::meta::type_of(member);
                if constexpr (!std::meta::is_reference_type(member_type)) {
                    using Inspector = detail::refl::MemberAnnotationInspector<member>;
                    if constexpr (!Inspector::is_schema_skip()) {
                        constexpr auto name = Inspector::get_name();
                        using MemberT = [: member_type :];
                        auto member_schema = generate_schema<MemberT>(mem);

                        // 提取成员级约束并追加
                        static constexpr auto member_annos = std::define_static_array(
                            std::meta::annotations_of(member)
                        );

                        template for (constexpr auto anno : member_annos) {
                            constexpr auto anno_t = std::meta::type_of(anno);
                            using AnnoT = [: anno_t :];
                            using DecayedAnno = std::decay_t<AnnoT>;
                            if constexpr (requires { DecayedAnno::trait; }) {
                                constexpr auto trait = DecayedAnno::trait;
                                constexpr static auto constant_val = [: std::meta::constant_of(anno) :];

                                if constexpr (trait == attr::Trait::SchemaRange) {
                                    pmr::string extra_str(mem);
                                    if (constant_val.min != attr::no_range) {
                                        extra_str += " MIN ";
                                        extra_str += std::to_string(constant_val.min);
                                    }
                                    if (constant_val.max != attr::no_range) {
                                        extra_str += " MAX ";
                                        extra_str += std::to_string(constant_val.max);
                                    }
                                    append_to_rule(member_schema, extra_str);
                                } else if constexpr (trait == attr::Trait::SchemaValidate) {
                                    pmr::string extra_str(" VALIDATE ", mem);
                                    extra_str += AnnoT::func();
                                    append_to_rule(member_schema, extra_str);
                                } else if constexpr (trait == attr::Trait::SchemaValidateArgs) {
                                    pmr::string extra_str(" VALIDATE ", mem);
                                    extra_str += AnnoT::func();
                                    if constexpr (AnnoT::arg_count > 0) {
                                        extra_str += " ( ";
                                        [&extra_str]<std::size_t... Is>(std::index_sequence<Is...>) {
                                            ( ... , (extra_str += "\"", extra_str += AnnoT::template get_arg<Is>().view(), extra_str += "\" ") );
                                        }(std::make_index_sequence<AnnoT::arg_count>{});
                                        extra_str += ")";
                                    }
                                    append_to_rule(member_schema, extra_str);
                                } else if constexpr (trait == attr::Trait::SchemaDefaultValue) {
                                    member_schema[1] = to_adata(constant_val.value, mem);
                                } else if constexpr (trait == attr::Trait::SchemaFallback) {
                                    append_to_rule(member_schema, " OVERRIDE_CONFLICT");
                                } else if constexpr (trait == attr::Trait::SchemaOptional) {
                                    append_to_rule(member_schema, " OPTIONAL");
                                } else if constexpr (trait == attr::Trait::SchemaExtra) {
                                    pmr::string extra_str(" ", mem);
                                    extra_str += AnnoT::rules();
                                    append_to_rule(member_schema, extra_str);
                                }
                            }
                        }

                        root[name] = std::move(member_schema);
                    }
                }
            }
        }

        return root;
    }

} // namespace alib6
