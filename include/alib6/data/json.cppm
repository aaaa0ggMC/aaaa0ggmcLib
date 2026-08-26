/**
 * @file json.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief JSON 序列化与反序列化策略接口 (alib6.data:json)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.data:json;
import std;
import alib6.core;
import :concepts;
import :kernel;

export namespace alib6::data {

    /**
     * @brief JSON 转储与解析配置
     */
    struct JSONConfig {
        enum FilterOp : u8 {
            Discard,
            Keep
        };

        constexpr static auto sort_asc = [](std::string_view a, std::string_view b) noexcept {
            return (a <=> b) == std::strong_ordering::less;
        };

        constexpr static auto sort_dsc = [](std::string_view a, std::string_view b) noexcept {
            return (a <=> b) == std::strong_ordering::greater;
        };

        using CompareFn = bool(std::string_view a, std::string_view b);
        using FilterFn = FilterOp(std::string_view key, const AData& node);

        // Dump 配置
        usize dump_indent{2};
        char dump_indent_char{' '};
        bool compact_lines{false};
        bool compact_spaces{false};
        bool ensure_ascii{false};
        bool warn_when_nan{true};
        int float_precision{-1};

        std::function<CompareFn> sort_object{nullptr};
        std::function<FilterFn> filter{nullptr};

        // Parse 配置
        bool rapidjson_recursive{true};
        bool allow_comments{false};
    };

    /**
     * @brief JSON 数据策略类 (符合 IsDataPolicy 概念)
     */
    struct JSON {
        using __dump_fn = void(std::string_view, void*);

        enum DumpResult : u8 {
            Success,
            EncounteredNAN,
            EncounteredINF
        };

        JSONConfig cfg;

        explicit JSON(const JSONConfig& c = JSONConfig()) : cfg(c) {}

        /**
         * @brief 将 JSON 字符串解析为 AData 树 (实现位于 modules/alib6/data/json.cpp)
         */
        bool parse(std::string_view data, AData& root);

        /**
         * @brief 内部转储流驱动函数 (实现位于 modules/alib6/data/json.cpp)
         */
        DumpResult __internal_dump(__dump_fn fn, void* p, const AData& root) const;

        /**
         * @brief 将 AData 转储为 JSON 字符串并追加至目标容器
         */
        template<class T>
        auto dump(T& target, const AData& root) const {
            auto fn = [](std::string_view sv, void* ag) {
                auto& out = *static_cast<T*>(ag);
                if constexpr (requires { out.append(sv); }) {
                    out.append(sv);
                } else if constexpr (requires { out += sv; }) {
                    out += sv;
                } else {
                    std::format_to(std::back_inserter(out), "{}", sv);
                }
            };
            return __internal_dump(fn, &target, root);
        }
    };

} // namespace alib6::data
