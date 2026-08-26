/**
 * @file entity.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ECS 实体核心定义 (轻量数值类型，包含 id 与版本计数，全特化哈希与格式化支持)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.ecs:entity;
import std;
import alib6.core;

export namespace alib6::ecs {

    /// @brief 实体唯一标识符类型
    using id_t = u64;

    /**
     * @brief 基础实体值类型
     * 包含全局自增 ID 与版本号计数器，用于槽位重用时的有效性与代际校验。
     */
    struct Entity {
        /// @brief 实体 ID (从 1 开始，0 表示空实体 / null)
        id_t id{0};
        /// @brief 实体的代际版本号
        u32 version{0};

        constexpr Entity() noexcept = default;
        constexpr Entity(id_t i_id, u32 i_version = 0) noexcept : id(i_id), version(i_version) {}

        /**
         * @brief 复用实体槽位时递增代际版本号
         */
        constexpr void reset() noexcept {
            ++version;
        }

        /**
         * @brief 创建一个语义明确的空实体 (null)
         */
        [[nodiscard]] static constexpr Entity null() noexcept {
            return Entity{0, 0};
        }

        /// @brief 检查当前实体是否为空 (ID == 0)
        [[nodiscard]] constexpr bool is_null() const noexcept {
            return id == 0;
        }

        /// @brief 检查当前实体是否有效 (ID != 0)
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return id != 0;
        }

        /// @brief 隐式布尔转换
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return id != 0;
        }

        constexpr auto operator<=>(const Entity&) const noexcept = default;
        constexpr bool operator==(const Entity&) const noexcept = default;

        template<class Target>
        void write_to_log(Target& target) const {
            if (is_null()) {
                target.append("Entity(null)");
            } else {
                std::format_to(std::back_inserter(target), "Entity(id: {}, v: {})", id, version);
            }
        }
    };

    //// 命名别名兼容 ////
    using entity = Entity;
    using entity_t = Entity;

} // namespace alib6::ecs

export namespace std {

    template<>
    struct hash<alib6::ecs::Entity> {
        constexpr std::size_t operator()(const alib6::ecs::Entity& e) const noexcept {
            std::size_t h1 = std::hash<alib6::ecs::id_t>{}(e.id);
            std::size_t h2 = std::hash<alib6::u32>{}(e.version);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    template<>
    struct formatter<alib6::ecs::Entity> : formatter<string_view> {
        template<typename FormatContext>
        auto format(const alib6::ecs::Entity& e, FormatContext& ctx) const {
            if (e.is_null()) {
                return formatter<string_view>::format("Entity(null)", ctx);
            }
            std::string buf = std::format("Entity(id: {}, v: {})", e.id, e.version);
            return formatter<string_view>::format(buf, ctx);
        }
    };

} // namespace std
