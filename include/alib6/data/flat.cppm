/**
 * @file flat.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 控制台/日志友好的扁平化文本策略接口 (alib6.data:flat)
 * @version 6.0
 * @date 2026-09-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <string_view>

export module alib6.data:flat;
import std;
import alib6.core;
import :concepts;
import :kernel;

export namespace alib6::data {

    /**
     * @brief Flat 扁平文本策略配置
     */
    struct FlatConfig {
        /// 键值分隔符
        std::string_view key_value_sep{"="};
        /// 展平路径分隔符
        std::string_view path_sep{"."};
        /// 条目分隔符
        std::string_view item_sep{","};
        /// 数组下标包围符 (array_bracket 为 true 时生效)
        std::string_view array_open{"["};
        std::string_view array_close{"]"};
        /// 数组下标使用方括号 (a[0]) 而非点号 (a.0)
        bool array_bracket{true};
        /// null 值的显示文本
        std::string_view null_text{"null"};
        /// 对象键是否排序输出 (保证输出可复现)
        bool sort_keys{true};
        /// 字符串是否始终加引号
        bool always_quote_string{false};
        /// 字符串在包含分隔符/空白/易混淆内容时自动加引号
        bool quote_when_needed{true};
        /// 浮点输出精度, <0 表示使用默认转换
        int float_precision{-1};
    };

    /**
     * @brief 扁平化文本数据策略类 (符合 IsDataPolicy 概念)
     *
     * 将 AData 树展平为 `a.b=1,c=2` 形式的单行文本, 适用于控制台与日志输出。
     * 嵌套对象使用路径分隔符展平 (如 `window.x=1`), 数组使用下标路径
     * (如 `points[0]=1`)。
     *
     * 典型用法:
     * @code
     * struct Window { u32 x; u32 y; };
     * auto text = to_adata(window).str<Flat>();  // "x=1,y=2"
     * @endcode
     *
     * @warning 该格式为**有损 (lossy)** 格式。解析 (parse) 时无法还原原始值的
     * 类型信息: 例如字符串 `"123"` 与整数 `123` 无法区分, 空对象/空数组与
     * null 也无法区分。转储端会尽可能对"看起来像标量"的字符串加引号以降低
     * 歧义, 但该格式仍仅应用于展示/调试, 或明确可接受类型推断损失的场景。
     * 需要无损往返时请使用 JSON 或 TOML。
     */
    struct Flat {
        using __dump_fn = void(std::string_view, void*);

        FlatConfig cfg;

        explicit Flat(const FlatConfig& c = FlatConfig()) : cfg(c) {}

        /**
         * @brief 将 Flat 文本解析为 AData 树 (实现位于 modules/alib6/data/flat.cpp)
         * @note 有损解析, 详见类型说明
         */
        bool parse(std::string_view data, AData& root);

        /**
         * @brief 内部转储流驱动函数 (实现位于 modules/alib6/data/flat.cpp)
         */
        void __internal_dump(__dump_fn fn, void* p, const AData& root) const;

        /**
         * @brief 将 AData 转储为 Flat 文本并追加至目标容器
         */
        template<class T>
        auto dump(T& target, const AData& root) const {
            auto fn = [](std::string_view sv, void* ag) {
                auto& out = *static_cast<T*>(ag);
                if constexpr (requires { out.append(sv); }) {
                    out.append(sv);
                } else if constexpr (requires { out += sv; }) {
                    out += sv;
                } else if constexpr (requires { out << sv; }) {
                    out << sv;
                } else {
                    std::format_to(std::back_inserter(out), "{}", sv);
                }
            };
            __internal_dump(fn, &target, root);
        }
    };

} // namespace alib6::data
