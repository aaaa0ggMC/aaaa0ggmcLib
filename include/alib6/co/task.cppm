/**
 * @file task.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 协程任务包装器 (alib6.co:task)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <generator>
#include <optional>
#include <type_traits>
#include <concepts>
#include <ranges>
#include <utility>

export module alib6.co:task;

import alib6.core;
import :concepts;

export namespace alib6::co {

    /**
     * @brief 基于 C++23 std::generator 的协程任务轻量封装
     */
    template<class T>
    struct Task {
        using gen_t = std::generator<T>;
        using iterator = decltype(std::declval<gen_t>().begin());

        gen_t handle;
        std::optional<iterator> current;
        bool inited{false};

        void reset() {
            current.reset();
            inited = false;
        }

        Task(Task&& other) noexcept
            : handle(std::move(other.handle)) {
            if (inited) alib6::panic("Coroutine has inited!");
            if (other.inited) alib6::panic("Other's coroutine has inited!");
            reset();
            inited = other.inited;
            other.inited = false;
            other.current.reset();
        }

        Task(gen_t&& t)
            : handle(std::move(t)) {}

        template<class Fn, class... Args>
            requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task> &&
                      std::is_lvalue_reference_v<Fn>)
        Task(Fn&& t, Args&&... args)
            : handle(std::forward<Fn>(t)(std::forward<Args>(args)...)) {}

        template<class Fn, class... Args>
            requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task> &&
                      !std::is_lvalue_reference_v<Fn> &&
                      requires(Fn f) { +f; })
        Task(Fn&& t, Args&&... args)
            : handle((+t)(std::forward<Args>(args)...)) {}

        template<class Fn, class... Args>
            requires (!std::is_same_v<std::remove_cvref_t<Fn>, Task> &&
                      !std::is_lvalue_reference_v<Fn> &&
                      !requires(Fn f) { +f; })
        Task(Fn&& t, Args&&... args) = delete;

        bool should_next() {
            if (!inited) return true;
            return *current != handle.end();
        }

        void next() {
            if (!inited) {
                inited = true;
                current.emplace(std::move(handle.begin()));
            } else [[likely]] {
                if (!should_next()) alib6::panic("Generator has terminated!");
                ++(*current);
            }
        }
    };

    template<class Fn, class... Args>
    Task(Fn&&, Args&&...) -> Task<std::ranges::range_value_t<std::remove_cvref_t<decltype(std::declval<Fn>()(std::declval<Args>()...))>>>;

    template<class T>
    Task(std::generator<T>&&) -> Task<T>;

    template<class T>
    Task(std::generator<T>&) -> Task<T>;

    template<class U>
    struct OnErr {
        U task;

        template<class T>
        OnErr(T&& v) : task(Task(std::forward<T>(v))) {}
    };

    template<class T>
    OnErr(T&& v) -> OnErr<decltype(Task(std::forward<T>(v)))>;

} // namespace alib6::co
