/**
 * @file data_json.h
 * @brief JSON Data Policy for ALib5 / ALib5 的 JSON 数据策略
 *
 * @details
 * English: This file provides the JSON parsing and dumping strategy for the ALib5 data kernel.
 * It defines `JSONConfig` for customizing processing behavior (indentation, compactness, filters, etc.)
 * and the `JSON` structure which utilizes RapidJSON to handle serialization and deserialization of `AData` nodes.
 * 
 * Chinese: 本文件提供了 ALib5 数据内核的 JSON 解析与转储（Dump）策略。
 * 它定义了用于自定义处理行为（缩进、紧凑度、过滤器等）的 `JSONConfig`，以及利用 RapidJSON 
 * 来处理 `AData` 节点序列化和反序列化的 `JSON` 结构体。
 */

#ifndef ALIB5_ADATA_PL_JSON
#define ALIB5_ADATA_PL_JSON

#include <alib5/data/kernel.h>
#include <functional>
#include <string_view>
#include <compare>

namespace alib5::data {

    /**
     * @brief Configuration for JSON parsing and dumping.
     */
    struct ALIB5_API JSONConfig {
        
        /**
         * @brief Enumeration for filtering operations during dump.
         */
        enum FilterOp {
            Discard, ///< Discard the node.
            Keep     ///< Keep the node.
        };

        /**
         * @brief Predefined ascending sort lambda for object keys.
         */
        constexpr static auto sort_asc = [](std::string_view a, std::string_view b) {
            return (a <=> b) == std::strong_ordering::less;
        };

        /**
         * @brief Predefined descending sort lambda for object keys.
         */
        constexpr static auto sort_dsc = [](std::string_view a, std::string_view b) {
            return (a <=> b) == std::strong_ordering::greater;
        };

        using CompareFn = bool(std::string_view a, std::string_view b);
        using FilterFn = FilterOp(std::string_view key, const dadata_t& node);

        // ---------------------------------------------------------
        // Dump Settings
        // ---------------------------------------------------------
        
        /**
         * @brief Number of indentation characters per level.
         * 
         * @par Original Comments:
         * English: Dump Settings.
         * Chinese: Dump Settings
         */
        size_t dump_indent { 2 };
        
        char dump_indent_char { ' ' };  ///< Character used for indentation.
        bool compact_lines { false };   ///< If true, removes line breaks for a compact output.
        bool compact_spaces { false };  ///< If true, removes extra spaces (e.g., around colons).
        bool ensure_ascii { false };    ///< If true, escapes non-ASCII characters.
        bool warn_when_nan { true };    ///< If true, emits a warning when encountering NaN/Inf values.
        int float_precision { -1 };     ///< Floating-point output precision. -1 means default format.
        
        std::function<CompareFn> sort_object { nullptr }; ///< Function to sort object keys before dumping.
        
        /**
         * @brief Filter function to dynamically omit nodes during dumping.
         * 
         * @par Original Comments:
         * English: Return true (Keep) to preserve, false (Discard) to discard.
         * Chinese: 返回true保留,false抛弃
         */
        std::function<FilterFn> filter { nullptr }; 

        // ---------------------------------------------------------
        // Parse Settings
        // ---------------------------------------------------------
        
        /**
         * @brief Parsing execution strategy regarding recursion depth.
         * 
         * @par Original Comments:
         * English: By default uses RapidJSON's default parse method -- recursive processing. Change to false to use an iterative simulated stack.
         * Chinese: 默认使用rapidjson默认parse方式--递归处理. 改为false使用模拟栈
         */
        bool rapidjson_recursive { true };
        
        /**
         * @brief Whether to allow comments in the JSON string during parsing.
         * 
         * @par Original Comments:
         * English: Support comments during parsing.
         * Chinese: 解析时支持注释
         */
        bool allow_comments { false };
    };

    /**
     * @brief JSON policy class implementing `IsDataPolicy` for `AData`.
     * 
     * @par Original Comments:
     * English: Since RapidJSON's SAX mode is used, it seems unnecessary to use the pImpl technique to cache context.
     * Chinese: 由于使用rapidjson的SAX模式,因此似乎也不需要使用pImpl技术缓存上下文
     */
    struct ALIB5_API JSON {
        using __dump_fn = void(std::string_view, void*);
        
        /**
         * @brief Result status of a dump operation.
         */
        enum DumpResult {
            Success,
            EncounteredNAN,
            EncounteredINF
        };

        JSONConfig cfg; ///< Configuration setting for the policy.
        
        /**
         * @brief Default constructor.
         * @param c Initial configuration settings.
         */
        JSON(const JSONConfig& c = JSONConfig()) : cfg(c) {}
        
        /**
         * @brief Parses JSON string data into an `AData` hierarchy.
         * 
         * @param data The JSON string format data.
         * @param root The root `AData` node to write parsed values into.
         * @return bool True if parsing succeeded, false if an error occurred.
         * 
         * @par Original Comments:
         * English: returns false if error
         * Chinese: 失败返回false
         */
        bool ALIB5_API parse(std::string_view data, dadata_t& root);
        
        /**
         * @brief Internal method handling the core dump rendering logic.
         */
        DumpResult ALIB5_API __internal_dump(__dump_fn fn, void* p, const dadata_t& root);

        /**
         * @brief Dumps an `AData` node to a target string container.
         * 
         * @tparam T The target string-like type (e.g., std::string, std::pmr::string).
         * @param target The container to which the serialized JSON text will be appended.
         * @param root The source `AData` root node.
         * @return DumpResult Status indicator of the dump operation.
         */
        template<IsStringLike T> 
        auto dump(T&& target, const dadata_t& root);
    };

} // namespace alib5::data

// ----------------------------------------------------------------------------------------------------
// Inline Implementations
// ----------------------------------------------------------------------------------------------------

namespace alib5::data {

    template<IsStringLike T> 
    inline auto JSON::dump(T&& target, const dadata_t& root) {
        auto fn = [](std::string_view sv, void* ag) {
            using type = std::decay_t<T>;
            type& out = *static_cast<type*>(ag);
            
            // Standard approach depending on append vs operator+= availability
            if constexpr (requires { out.append(sv); }) {
                out.append(sv);
            } else {
                out += sv; 
            }
        };
        
        return __internal_dump(fn, &target, root);
    }

} // namespace alib5::data

#endif