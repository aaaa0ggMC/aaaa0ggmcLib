/**
 * @file pool.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ECS 组件存储池、依赖链元组与全 PMR 内存管理
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.ecs:pool;
import std;
import alib6.core;
import :entity;
import :concepts;

namespace pmr = std::pmr;

export namespace alib6::ecs {

    /// @brief ECS 组件安全引用包装器 (基于 FreelistLinearStorage)
    template<class T>
    using ref_t = alib6::RefWrapper<alib6::storage::FreelistLinearStorage<T>>;

    namespace detail {
        /**
         * @brief 历史兼容的循环依赖检测器模板
         */
        template<bool Already, class Compare, class... Ts>
        struct CycleChecker;

        template<bool Already, class Compare, class T, class... Ts>
        struct CycleChecker<Already, Compare, T, Ts...> {
            static constexpr bool matched = std::is_same_v<Compare, T>;
            static constexpr bool next_already = Already || matched;
            static constexpr bool conflict = (Already && matched) || CycleChecker<next_already, Compare, Ts...>::conflict;
        };

        template<bool Already, class Compare, class T>
        struct CycleChecker<Already, Compare, T> {
            static constexpr bool matched = std::is_same_v<Compare, T>;
            static constexpr bool conflict = (Already && matched);
        };

        template<bool Already, class Compare>
        struct CycleChecker<Already, Compare> {
            static constexpr bool conflict = false;
        };
    } // namespace detail

    /**
     * @brief 递归组件依赖栈 (用于拓扑依赖注入与编译期循环依赖防御)
     */
    template<class... Ts>
    struct ComponentStack {
        /// @brief 向依赖栈头部新增组件类型
        template<class T>
        using add_t = ComponentStack<T, Ts...>;

        /// @brief 对应的 safe ref tuple 类型
        using deptup_t = std::tuple<ref_t<Ts>...>;

        /**
         * @brief 编译期检查新增类型 T 是否会造成循环依赖 (利用 C++26 折叠表达式实现 0 开销瞬时展开)
         */
        template<class T>
        [[nodiscard]] static constexpr bool check_cycle() noexcept {
            return (std::is_same_v<T, Ts> || ...);
        }
    };

    /**
     * @brief 类型擦除的组件池抽象基类 (全面支持 PMR 内存注入与多态析构释放)
     */
    struct ComponentPoolBase {
        virtual ~ComponentPoolBase() = default;

        /**
         * @brief 销毁指定实体关联的组件
         * @return 0 成功, -1 未找到
         */
        virtual int destroy(id_t entity_id) = 0;

        /**
         * @brief 判定指定实体是否包含该组件
         */
        [[nodiscard]] virtual bool has(id_t entity_id) const noexcept = 0;

        /**
         * @brief 当前池中有效存活组件数量
         */
        [[nodiscard]] virtual usize size() const noexcept = 0;

        /**
         * @brief 清空当前组件池
         */
        virtual void clear() = 0;

        /**
         * @brief 获取分配器内存资源
         */
        [[nodiscard]] virtual memory_resource* get_allocator() const noexcept = 0;

        /**
         * @brief 通过 PMR 多态内存资源就地析构并归还池对象自身所占内存
         */
        virtual void destroy_self(memory_resource* m) = 0;
    };

    /**
     * @brief 具体类型组件池 (基于 FreelistLinearStorage 与 pmr::unordered_map 实现 O(1) 索引与紧凑存储)
     */
    template<class T>
    struct ComponentPool : public ComponentPoolBase {
        using StorageType = alib6::storage::FreelistLinearStorage<T>;

        /// @brief 组件底层线性存储池
        StorageType data;
        /// @brief Entity ID -> 组件槽位索引的哈希映射 (PMR 隔离)
        pmr::unordered_map<id_t, usize> mapper;
        /// @brief 绑定的内存资源指针
        memory_resource* mem_res{nullptr};

        /// @brief 历史兼容的函数指针式销毁器
        int (*destroyer)(void*, id_t){nullptr};

        explicit ComponentPool(usize reserve_size = 0, memory_resource* mem = get_default_resource())
            : data(reserve_size, mem), mapper(mem), mem_res(mem) {
            destroyer = [](void* self, id_t eid) -> int {
                return static_cast<ComponentPool<T>*>(self)->destroy(eid);
            };
        }

        ~ComponentPool() override = default;

        [[nodiscard]] memory_resource* get_allocator() const noexcept override {
            return mem_res;
        }

        void destroy_self(memory_resource* m) override {
            this->~ComponentPool();
            m->deallocate(this, sizeof(ComponentPool<T>), alignof(ComponentPool<T>));
        }

        int destroy(id_t entity_id) override {
            auto it = mapper.find(entity_id);
            if (it == mapper.end()) return -1;

            usize slot = it->second;
            if constexpr (ComponentTraits<T>::cleanup) {
                data[slot].cleanup();
            }
            if constexpr (ComponentTraits<T>::bind) {
                data[slot].bind(Entity::null());
            }
            data.remove(slot);
            mapper.erase(it);
            return 0;
        }

        [[nodiscard]] bool has(id_t entity_id) const noexcept override {
            return mapper.contains(entity_id);
        }

        [[nodiscard]] usize size() const noexcept override {
            return data.occupied_count();
        }

        void clear() override {
            if constexpr (ComponentTraits<T>::cleanup) {
                for (const auto& [eid, slot] : mapper) {
                    data[slot].cleanup();
                }
            }
            data.clear();
            mapper.clear();
        }

        /**
         * @brief 尝试获取实体对应的组件槽位索引
         */
        [[nodiscard]] std::optional<usize> try_get_slot(id_t entity_id) const noexcept {
            auto it = mapper.find(entity_id);
            if (it == mapper.end()) return std::nullopt;
            return it->second;
        }
    };

} // namespace alib6::ecs
