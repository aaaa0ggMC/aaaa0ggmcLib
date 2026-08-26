/**
 * @file concepts.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 协程概念定义 (alib6.co:concepts)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <concepts>
#include <type_traits>

export module alib6.co:concepts;

export namespace alib6::co {

    /**
     * @brief 检查类型是否支持 until() 条件判断
     */
    template<class T>
    concept CanUntil = requires(T& t) {
        { t.until() } -> std::convertible_to<bool>;
    };

    template<class T>
    struct Task;

    /**
     * @brief 检查类型是否为合法的 Task 任务包装
     */
    template<class T>
    concept IsTask = requires(T&& t) {
        Task(t);
    } || requires(std::remove_cvref_t<T>& t) {
        { t.next() } -> std::same_as<void>;
        { t.should_next() } -> std::convertible_to<bool>;
    };

} // namespace alib6::co
