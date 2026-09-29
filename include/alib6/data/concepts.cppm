/**
 * @file concepts.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 动态数据结构概念与枚举策略定义 (alib6.data:concepts)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.data:concepts;
import std;
import alib6.core;

export namespace alib6::data {

    /**
     * @brief 允许作为 AData 值节点的合法类型概念
     */
    template<class T>
    concept IsNodeValue = 
        std::integral<std::decay_t<T>> ||
        std::floating_point<std::decay_t<T>> ||
        std::same_as<std::decay_t<T>, bool> ||
        alib6::Cast<std::decay_t<T>, std::string_view> ||
        alib6::Cast<T, std::string_view>;

    /**
     * @brief 数据解析策略概念 (从文本构建 Data)
     */
    template<class T, class Data>
    concept IsDataParser = requires(T& t, std::string_view data, Data& d) {
        t.parse(data, d);
    };

    /**
     * @brief 数据转储策略概念 (将 Data 渲染为文本)
     */
    template<class T, class Data>
    concept IsDataDumper = requires(const T& t, std::pmr::string& dmp, const Data& cd) {
        t.dump(dmp, cd);
    };

    /**
     * @brief 数据序列化/反序列化策略概念 (解析与转储均支持)
     */
    template<class T, class Data>
    concept IsDataPolicy = IsDataParser<T, Data> && IsDataDumper<T, Data>;

    /**
     * @brief 合并操作策略
     */
    enum class MergeOperation : i64 {
        Override,
        Skip
    };

    /**
     * @brief 合并仿函数概念
     */
    template<class T, class Data>
    concept IsMergeFn = requires(T& t, Data& dest, const Data& src) {
        { t(dest, src) } -> std::convertible_to<MergeOperation>;
    };

    /**
     * @brief 差异比对仿函数概念
     */
    template<class T, class Data>
    concept IsDiffFn = requires(T& t, const Data& dest, const Data& src) {
        { t(dest, src) } -> std::convertible_to<MergeOperation>;
    };

    /**
     * @brief 剪枝仿函数概念
     */
    template<class T, class Data>
    concept IsPruneFn = requires(T& t, const Data& dest) {
        { t(dest) } -> std::convertible_to<bool>;
    };

    /**
     * @brief 值比较模糊度策略
     */
    enum class CompareStrategy : i64 {
        Strict,     ///< 严格限制类型与数值完全一致
        BoolStrict, ///< 允许 INT 和 DOUBLE 互相比较；Bool 仅支持 0 或 1
        Lesser,     ///< 模拟 C/C++ 隐式 bool 规则
        Fuzzy       ///< 允许跨类型模糊转换与比对 (如字符串数字 "123" 与整型 123)
    };

} // namespace alib6::data
