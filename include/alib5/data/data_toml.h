/**
 * @file data_toml.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief TOML Data Policy for ALib5 / ALib5 的 TOML 数据策略
 * @version 5.0
 * @date 2026-06-10
 * 
 * @copyright Copyright (c) 2026
 * 
 * @details
 * English: This file provides the TOML parsing strategy for the ALib5 data kernel.
 * Please note that TOML dumping (serialization) is currently unsupported.
 * 
 * Chinese: 本文件提供了 ALib5 数据内核的 TOML 解析策略。
 * 请注意，目前不支持 TOML 转储（序列化）功能。
 * 
 * @par Original Comments:
 * English: Angry toml does not yet support delicious dump.
 * Chinese: 愤怒的toml还不支持美味的dump
 */

#ifndef ALIB5_ADATA_PL_TOML
#define ALIB5_ADATA_PL_TOML

#include <alib5/data/kernel.h>

namespace alib5::data {

    /**
     * @brief Configuration for TOML parsing and dumping.
     * 
     * @details Currently empty as parsing utilizes default `toml++` behavior, 
     * and dumping is not yet implemented.
     */
    struct ALIB5_API TOMLConfig {
        
    };

    /**
     * @brief TOML policy class implementing `IsDataPolicy` for `AData`.
     */
    struct ALIB5_API TOML {
        using __dump_fn = void(std::string_view, void*);

        TOMLConfig cfg; ///< Configuration settings for the policy.
        
        /**
         * @brief Default constructor.
         * @param c Initial configuration settings.
         */
        TOML(const TOMLConfig& c = TOMLConfig()) : cfg(c) {}
        
        /**
         * @brief Parses TOML string data into an `AData` hierarchy.
         * 
         * @param data The TOML string format data.
         * @param node The root `AData` node to write parsed values into.
         * @return bool True if parsing succeeded, false if an error occurred.
         * 
         * @par Original Comments:
         * English: returns false if error.
         * Chinese: 如果发生错误则返回 false。
         */
        bool ALIB5_API parse(std::string_view data, dadata_t& node);
        
        /**
         * @brief Internal method handling the core dump rendering logic.
         * 
         * @warning This function is deprecated and currently unimplemented. Calling it will trigger a panic.
         */
        [[deprecated("This function hasn't been implemented")]]
        void ALIB5_API __internal_dump(__dump_fn fn, void* p, const dadata_t& root);

        /**
         * @brief Dumps an `AData` node to a target string container.
         * 
         * @tparam T The target string-like type (e.g., std::string, std::pmr::string).
         * @param target The container to which the serialized TOML text will be appended.
         * @param root The source `AData` root node.
         * 
         * @warning This function is deprecated and currently unimplemented. Calling it will trigger a panic.
         */
        template<IsStringLike T> 
        [[deprecated("This function hasn't been implemented")]]
        auto dump(T&& target, const dadata_t& root);
    };

} // namespace alib5::data

// ----------------------------------------------------------------------------------------------------
// Inline Implementations
// ----------------------------------------------------------------------------------------------------

namespace alib5::data {

    template<IsStringLike T> 
    inline auto TOML::dump(T&& target, const dadata_t& root) {
        auto fn = [](std::string_view sv, void* ag) {
            using type = std::decay_t<T>;
            type& out = *static_cast<type*>(ag);
            
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