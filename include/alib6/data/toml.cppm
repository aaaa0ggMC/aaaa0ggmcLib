/**
 * @file toml.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief TOML 解析与转储策略接口 (alib6.data:toml)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.data:toml;
import std;
import alib6.core;
import :concepts;
import :kernel;

export namespace alib6::data {

    /**
     * @brief TOML 处理配置
     */
    struct TOMLConfig {
        
    };

    /**
     * @brief TOML 数据策略类 (符合 IsDataPolicy 概念)
     */
    struct TOML {
        using __dump_fn = void(std::string_view, void*);

        TOMLConfig cfg;

        explicit TOML(const TOMLConfig& c = TOMLConfig()) : cfg(c) {}

        /**
         * @brief 将 TOML 文本解析为 AData 树 (实现位于 modules/alib6/data/toml.cpp)
         */
        bool parse(std::string_view data, AData& node);

        /**
         * @brief 内部转储流驱动函数 (实现位于 modules/alib6/data/toml.cpp)
         */
        void __internal_dump(__dump_fn fn, void* p, const AData& root) const;

        /**
         * @brief 将 AData 转储为 TOML 文本并追加至目标容器
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
            __internal_dump(fn, &target, root);
        }
    };

} // namespace alib6::data
