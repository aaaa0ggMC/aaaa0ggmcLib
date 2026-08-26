/**
 * @file concepts.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ECS 组件约束概念、依赖特征萃取与辅助基类
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.ecs:concepts;
import std;
import alib6.core;
import :entity;

namespace pmr = std::pmr;

export namespace alib6::ecs {

    /**
     * @brief 声明了 cleanup() 成员函数的组件概念 (移除组件时自动触发辅助资源即时回收)
     */
    template<class T>
    concept NeedCleanup = requires(T& t) {
        t.cleanup();
    };

    /**
     * @brief 空标记基类，继承该类表示需要自定义清理
     */
    struct INeedCleanup {};

    /**
     * @brief 声明了 reset(Args...) 成员函数的组件概念 (复用已有空闲槽位时就地重置初始化)
     */
    template<class T, class... Args>
    concept NeedReset = requires(T& t, Args&&... args) {
        t.reset(std::forward<Args>(args)...);
    };

    /**
     * @brief 空标记基类，继承该类表示支持空参数 reset
     */
    struct INeedReset {};

    /**
     * @brief 声明了 d_cleanup() 成员函数的组件概念 (底层线性存储释放槽位时触发)
     */
    template<class T>
    concept NeedDataCleanup = requires(T& t) {
        t.d_cleanup();
    };

    /**
     * @brief 空标记基类，继承该类表示支持底层数据清理
     */
    struct INeedDataCleanup {};

    /**
     * @brief 声明了 bind(Entity) 成员函数的组件概念 (由 EntityManager 在组件挂载/卸载时自动注入所属实体)
     */
    template<class T>
    concept NeedBind = requires(T& t, Entity e) {
        t.bind(e);
    };

    /**
     * @brief 辅助基类：实现对所属实体的自动绑定与存储
     */
    struct IBindEntity {
        Entity bound_entity{Entity::null()};

        constexpr void bind(const Entity& e) noexcept {
            bound_entity = e;
        }

        [[nodiscard]] constexpr const Entity& get_bound() const noexcept {
            return bound_entity;
        }
    };

    /**
     * @brief 声明了 slot(usize) 成员函数的组件概念 (由 EntityManager 注入组件在其存储池中的槽位索引)
     */
    template<class T>
    concept NeedSlotId = requires(T& t, usize index) {
        t.slot(index);
    };

    /**
     * @brief 辅助基类：实现组件槽位索引的自动绑定与存储
     */
    struct ISlotComponent {
        usize m_slot{std::numeric_limits<usize>::max()};

        constexpr void slot(usize i) noexcept {
            m_slot = i;
        }

        [[nodiscard]] constexpr usize get_slot() const noexcept {
            return m_slot;
        }
    };

    /**
     * @brief 声明了 bind_dep(Tup&) 成员函数的组件概念 (依赖项注入)
     */
    template<class T, class Tup>
    concept NeedBindDependency = requires(T& t, Tup& tup) {
        t.bind_dep(tup);
    };

    /**
     * @brief 声明了 Dependency 类型别名的组件概念
     * 示例: using Dependency = alib6::ecs::ComponentStack<Velocity, RigidBody>;
     */
    template<class T>
    concept NeedDependency = requires {
        typename T::Dependency;
    };

    /**
     * @brief 声明了 update(Args...) 成员函数的组件概念
     */
    template<class T, class... Args>
    concept NeedUpdate = requires(T& t, Args&&... args) {
        t.update(std::forward<Args>(args)...);
    };

    /**
     * @brief 组件能力特征萃取器 (编译期判定组件支持的生命周期及注入协议)
     */
    template<class T>
    struct ComponentTraits {
        /// @brief 是否声明了依赖列表
        static constexpr bool dependency = NeedDependency<T>;

        /// @brief 判定是否支持 bind_dep 注入
        static constexpr bool bind_dependency = [] {
            if constexpr (dependency) {
                if constexpr (requires { typename T::Dependency::deptup_t; }) {
                    return NeedBindDependency<T, typename T::Dependency::deptup_t>;
                }
            }
            return false;
        }();

        /// @brief 是否支持 cleanup() 清理钩子
        static constexpr bool cleanup = NeedCleanup<T>;
        /// @brief 是否支持 bind(Entity) 实体注入
        static constexpr bool bind = NeedBind<T>;
        /// @brief 是否支持 slot(usize) 槽位注入
        static constexpr bool slot_id = NeedSlotId<T>;

        /// @brief 是否支持特定参数签名的 reset(...)
        template<class... Args>
        static constexpr bool reset = NeedReset<T, Args...>;

        /// @brief 是否支持特定参数签名的 update(...)
        template<class... Args>
        static constexpr bool update = NeedUpdate<T, Args...>;

        /**
         * @brief 格式化输出组件特征到目标容器 (如 std::string, pmr::string, std::vector<char>)
         */
        template<class Tg>
        static void write_to_log(Tg& target) {
            auto yn = [](bool val) constexpr -> const char* { return val ? "Y" : "N"; };
            std::format_to(
                std::back_inserter(target),
                "\n{}:\n"
                "\tDependency            :{}\n"
                "\tCleanup               :{}\n"
                "\tBindEntity            :{}\n"
                "\tBindSlotId            :{}\n"
                "\tBindDependency        :{}\n"
                "\tHasEmptyReset         :{}\n"
                "\tHasEmptyUpdate        :{}",
                typeid(T).name(),
                yn(dependency),
                yn(cleanup),
                yn(bind),
                yn(slot_id),
                yn(bind_dependency),
                yn(requires(T& t) { t.reset(); }),
                yn(requires(T& t) { t.update(); })
            );
        }

        /**
         * @brief 将组件特征输出到流对象 (如 std::ostream)
         */
        template<class Target, class EndToken = std::string_view>
        static void check(Target&& t, EndToken&& end_token = "\n") {
            auto yn = [](bool val) constexpr -> const char* { return val ? "Y" : "N"; };
            std::forward<Target>(t) << typeid(T).name() << ":\n"
                "\tDependency            :" << yn(dependency) << "\n"
                "\tCleanup               :" << yn(cleanup) << "\n"
                "\tBindEntity            :" << yn(bind) << "\n"
                "\tBindSlotId            :" << yn(slot_id) << "\n"
                "\tBindDependency        :" << yn(bind_dependency) << "\n"
                "\tHasEmptyReset         :" << yn(requires(T& t) { t.reset(); }) << "\n"
                "\tHasEmptyUpdate        :" << yn(requires(T& t) { t.update(); })
                << std::forward<EndToken>(end_token);
        }

        /**
         * @brief 格式化生成当前组件特征的文本摘要 (PMR 兼容)
         */
        static pmr::string summary(memory_resource* mem = get_default_resource()) {
            auto yn = [](bool val) constexpr -> const char* { return val ? "YES" : "NO"; };
            pmr::string buf(mem);
            std::format_to(
                std::back_inserter(buf),
                "ComponentTraits<{}>:\n"
                "  Dependency     : {}\n"
                "  BindDependency : {}\n"
                "  Cleanup        : {}\n"
                "  BindEntity     : {}\n"
                "  SlotId         : {}\n"
                "  EmptyReset     : {}\n"
                "  EmptyUpdate    : {}",
                typeid(T).name(),
                yn(dependency),
                yn(bind_dependency),
                yn(cleanup),
                yn(bind),
                yn(slot_id),
                yn(requires(T& t) { t.reset(); }),
                yn(requires(T& t) { t.update(); })
            );
            return buf;
        }
    };

} // namespace alib6::ecs
