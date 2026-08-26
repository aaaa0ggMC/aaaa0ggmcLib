/**
 * @file view.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ECS 多组件联合查询视图 (类似 EnTT/Flecs 架构，自动选取最小基准池跳步过滤，支持 for_each 与解构迭代)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.ecs:view;
import std;
import alib6.core;
import :entity;
import :concepts;
import :pool;

namespace pmr = std::pmr;

export namespace alib6::ecs {

    struct EntityManager;

    /**
     * @brief 多组件复合查询视图
     * 自动推导最小组件池并执行 O(1) 过滤，具备极致的复合查询与遍历性能。
     * 
     * @tparam ...Components 待查询的组件类型列表
     */
    template<class... Components>
    struct View {
        EntityManager* em{nullptr};

        explicit constexpr View(EntityManager& manager) noexcept : em(&manager) {}

        /**
         * @brief 迭代器支持 (支持结构化绑定 for (auto [e, compA, compB] : view))
         */
        struct Iterator {
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = std::tuple<Entity, Components&...>;

            EntityManager* em{nullptr};
            std::tuple<ComponentPool<Components>*...> pools{};
            pmr::unordered_map<id_t, usize>::const_iterator current_it{};
            pmr::unordered_map<id_t, usize>::const_iterator end_it{};

            Iterator() = default;

            Iterator(
                EntityManager* m,
                const std::tuple<ComponentPool<Components>*...>& p,
                pmr::unordered_map<id_t, usize>::const_iterator cur,
                pmr::unordered_map<id_t, usize>::const_iterator end
            ) : em(m), pools(p), current_it(cur), end_it(end) {
                advance_to_valid();
            }

            void advance_to_valid();

            decltype(auto) operator*() const;

            Iterator& operator++() {
                if (current_it != end_it) {
                    ++current_it;
                    advance_to_valid();
                }
                return *this;
            }

            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const noexcept {
                return current_it == other.current_it;
            }

            bool operator!=(const Iterator& other) const noexcept {
                return current_it != other.current_it;
            }
        };

        /**
         * @brief 高性能遍历查询 (支持 func(Entity, Components&...) 或 func(Components&...))
         */
        template<class F>
        void for_each(F&& func) const;

        /**
         * @brief 获取当前满足条件的实体总数
         */
        [[nodiscard]] usize count() const {
            usize total = 0;
            for_each([&](const auto&...) {
                ++total;
            });
            return total;
        }

        /**
         * @brief 判定当前视图是否为空
         */
        [[nodiscard]] bool empty() const {
            return count() == 0;
        }

        [[nodiscard]] Iterator begin() const;
        [[nodiscard]] Iterator end() const;
    };

} // namespace alib6::ecs
