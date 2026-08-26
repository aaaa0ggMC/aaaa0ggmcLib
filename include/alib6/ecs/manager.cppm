/**
 * @file manager.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ECS 实体管理器与实体句柄包装器 (全生命周期 PMR、编译期依赖注入、复合视图查询)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>

export module alib6.ecs:manager;
import std;
import alib6.core;
import :entity;
import :concepts;
import :pool;
import :view;

namespace pmr = std::pmr;

export namespace alib6::ecs {

    class EntityWrapper;

    /**
     * @brief ECS 核心实体管理器
     */
    struct EntityManager {
    private:
        friend class EntityWrapper;

        struct PoolDeleter {
            memory_resource* mem{nullptr};

            void operator()(ComponentPoolBase* ptr) const {
                if (ptr) {
                    ptr->destroy_self(mem);
                }
            }
        };

        using PoolPtr = std::unique_ptr<ComponentPoolBase, PoolDeleter>;

        /// @brief 组件池映射表 (type_hash -> ComponentPool)
        pmr::unordered_map<usize, PoolPtr> component_pools;
        /// @brief 实体线性存储槽位池
        alib6::storage::FreelistLinearStorage<Entity> entities;
        /// @brief 当前分配的最大实体 ID (从 1 起步)
        id_t id_max{0};
        /// @brief 新建组件池时的默认预留槽位大小
        usize pool_reserve_size{0};
        /// @brief PMR 内存资源
        memory_resource* mem_res{nullptr};

    public:
        /// @brief 移除组件的结果状态码
        enum DestroyResult {
            DRSuccess  = 0, ///< 成功删除组件
            DRNoPool   = 1, ///< 不存在该类型的组件池
            DRCantFind = 2  ///< 组件池中未关联该实体
        };

        /**
         * @brief 构造 EntityManager 并配置预留容量与内存资源
         * @param entity_reserve_size 实体存储预留大小
         * @param pool_res 新组件池预留大小
         * @param mem 注入的多态内存资源
         */
        explicit EntityManager(
            usize entity_reserve_size = 0,
            usize pool_res = 0,
            memory_resource* mem = get_default_resource()
        ) : component_pools(mem)
          , entities(entity_reserve_size, mem)
          , id_max(0)
          , pool_reserve_size(pool_res)
          , mem_res(mem) {}

        ~EntityManager() = default;

        EntityManager(const EntityManager&) = delete;
        EntityManager& operator=(const EntityManager&) = delete;

        EntityManager(EntityManager&& other) noexcept
            : component_pools(std::move(other.component_pools))
            , entities(std::move(other.entities))
            , id_max(other.id_max)
            , pool_reserve_size(other.pool_reserve_size)
            , mem_res(other.mem_res) {
            other.id_max = 0;
        }

        EntityManager& operator=(EntityManager&& other) noexcept {
            if (this == &other) return *this;
            clear();
            component_pools = std::move(other.component_pools);
            entities = std::move(other.entities);
            id_max = other.id_max;
            pool_reserve_size = other.pool_reserve_size;
            mem_res = other.mem_res;
            other.id_max = 0;
            return *this;
        }

        [[nodiscard]] memory_resource* get_allocator() const noexcept {
            return mem_res;
        }

        // ==================== 实体生命周期管理 ====================

        /**
         * @brief 创建一个新实体 (优先复用空闲代际槽位)
         */
        Entity create_entity() {
            if (entities.free_elements.empty()) {
                return entities.next(++id_max, 0);
            } else {
                return entities.next_free();
            }
        }

        /**
         * @brief 判定实体是否在当前管理器中有效存在 (ID 有效且版本号匹配)
         */
        [[nodiscard]] bool is_valid(const Entity& e) const noexcept {
            if (e.id == 0 || e.id > entities.size()) return false;
            if (entities.available_bits.get(e.id - 1)) return false;
            return entities[e.id - 1].version == e.version;
        }

        /**
         * @brief 根据实体 ID 查询当前的最新实体句柄
         */
        [[nodiscard]] Entity get_entity_by_id(id_t eid) const noexcept {
            if (eid == 0 || eid > entities.size()) return Entity::null();
            if (entities.available_bits.get(eid - 1)) return Entity::null();
            return entities[eid - 1];
        }

        /**
         * @brief 销毁实体并级联回收其挂载的所有组件
         */
        void destroy_entity(const Entity& e) {
            if (!is_valid(e)) return;
            for (auto& [type_hash, pool] : component_pools) {
                pool->destroy(e.id);
            }
            entities.remove(e.id - 1);
        }

        /**
         * @brief 获取当前有效存活的实体总数
         */
        [[nodiscard]] usize entity_count() const noexcept {
            return entities.occupied_count();
        }

        /**
         * @brief 获取实体线性存储的总槽位大小
         */
        [[nodiscard]] usize get_entity_pool_size() const noexcept {
            return entities.size();
        }

        /**
         * @brief 清空所有实体与组件池数据
         */
        void clear() {
            for (auto& [type_hash, pool] : component_pools) {
                pool->clear();
            }
            entities.clear();
            id_max = 0;
        }

        auto& get_entities_storage() noexcept { return entities; }
        const auto& get_entities_storage() const noexcept { return entities; }

        // ==================== 组件池检索与注册 ====================

        /**
         * @brief 获取指定类型的组件池指针 (未注册返回 nullptr)
         */
        template<class T>
        [[nodiscard]] ComponentPool<T>* get_component_pool() const noexcept {
            auto hash_code = typeid(T).hash_code();
            auto it = component_pools.find(hash_code);
            if (it == component_pools.end()) return nullptr;
            return static_cast<ComponentPool<T>*>(it->second.get());
        }

        /**
         * @brief 确保指定类型的组件池已创建并返回指针 (全 PMR 内存分配)
         */
        template<class T>
        ComponentPool<T>* add_component_pool() {
            auto hash_code = typeid(T).hash_code();
            auto it = component_pools.find(hash_code);
            if (it != component_pools.end()) {
                return static_cast<ComponentPool<T>*>(it->second.get());
            }

            void* raw = mem_res->allocate(sizeof(ComponentPool<T>), alignof(ComponentPool<T>));
            auto* pool = ::new (raw) ComponentPool<T>(pool_reserve_size, mem_res);
            PoolPtr ptr(pool, PoolDeleter{mem_res});

            auto [ins_it, _] = component_pools.emplace(hash_code, std::move(ptr));
            return static_cast<ComponentPool<T>*>(ins_it->second.get());
        }

        /**
         * @brief 确保并获取组件池引用
         */
        template<class T>
        ComponentPool<T>& get_or_create_pool() {
            return *add_component_pool<T>();
        }

        // ==================== 组件挂载、查询与移除 ====================

        /**
         * @brief 判定实体是否挂载了指定组件
         */
        template<class T>
        [[nodiscard]] bool has_component(const Entity& e) const noexcept {
            if (!is_valid(e)) return false;
            auto* p = get_component_pool<T>();
            if (!p) return false;
            return p->has(e.id);
        }

        /**
         * @brief 获取实体组件的原生指针 (找不到或实体无效返回 nullptr)
         */
        template<class T>
        [[nodiscard]] T* get_component_raw(const Entity& e) const noexcept {
            if (!is_valid(e)) return nullptr;
            auto* p = get_component_pool<T>();
            if (!p) return nullptr;
            auto slot_opt = p->try_get_slot(e.id);
            if (!slot_opt) return nullptr;
            return std::addressof(p->data[*slot_opt]);
        }

        /**
         * @brief 获取实体组件的安全防悬垂引用 (找不到返回 std::nullopt)
         */
        template<class T>
        [[nodiscard]] std::optional<ref_t<T>> get_component(const Entity& e) {
            if (!is_valid(e)) return std::nullopt;
            auto* p = get_component_pool<T>();
            if (!p) return std::nullopt;
            auto slot_opt = p->try_get_slot(e.id);
            if (!slot_opt) return std::nullopt;
            return alib6::ref(p->data, *slot_opt);
        }

        /**
         * @brief 向实体挂载组件 (支持拓扑依赖注入、编译期循环依赖检查、实体/槽位注入)
         */
        template<class T, class Tuo = ComponentStack<>, class... Args>
        ref_t<T> add_component(const Entity& e, Args&&... args) {
            panic_if(!is_valid(e), "EntityManager::add_component called on invalid or destroyed Entity!");

            auto* p = add_component_pool<T>();
            auto slot_opt = p->try_get_slot(e.id);
            if (slot_opt) {
                return alib6::ref(p->data, *slot_opt);
            }

            auto create = [&](auto&& deps) -> ref_t<T> {
                bool flag = false;
                usize index = 0;
                T& comp = p->data.try_next_with_index(flag, index, std::forward<Args>(args)...);

                if constexpr (ComponentTraits<T>::bind) {
                    comp.bind(e);
                }
                if constexpr (ComponentTraits<T>::dependency) {
                    if constexpr (ComponentTraits<T>::bind_dependency) {
                        comp.bind_dep(deps);
                    }
                }
                if constexpr (ComponentTraits<T>::slot_id) {
                    if (flag) {
                        comp.slot(index);
                    }
                }
                p->mapper.emplace(e.id, index);
                return alib6::ref(p->data, index);
            };

            if constexpr (ComponentTraits<T>::dependency) {
                using Tuo_Next = typename Tuo::template add_t<T>;
                static_assert(!Tuo::template check_cycle<T>(), "Cyclic component dependency detected!");

                typename T::Dependency::deptup_t deps = []<class... TArgs>(
                    EntityManager& em,
                    const Entity& ent,
                    ComponentStack<TArgs...>
                ) {
                    return std::make_tuple(em.add_component<TArgs, Tuo_Next>(ent)...);
                }(*this, e, typename T::Dependency{});

                return create(deps);
            } else {
                return create(std::ignore);
            }
        }

        /**
         * @brief 从实体中移除指定组件
         */
        template<class T>
        DestroyResult remove_component(const Entity& e) {
            if (!is_valid(e)) return DRCantFind;
            auto* p = get_component_pool<T>();
            if (!p) return DRNoPool;
            if (p->destroy(e.id) == -1) return DRCantFind;
            return DRSuccess;
        }

        // ==================== 单类型更新遍历 ====================

        /**
         * @brief 遍历指定类型的所有有效组件并执行 f(comp)
         */
        template<class T, class F>
            requires std::invocable<F, T&> || std::invocable<F, usize, T&>
        void update(F&& f) {
            auto* pool = get_component_pool<T>();
            if (pool) {
                pool->data.for_each(std::forward<F>(f));
            }
        }

        /**
         * @brief 对指定类型的所有有效组件统一派发 comp.update(args...)
         */
        template<class T, class... Args>
            requires (ComponentTraits<T>::template update<Args...>)
        void update(Args&&... args) {
            auto* pool = get_component_pool<T>();
            if (!pool) return;
            pool->data.for_each([&](T& item) {
                item.update(args...);
            });
        }

        // ==================== 复合查询视图与句柄工厂 ====================

        /**
         * @brief 构建多组件联合查询视图 (自动按最小池跳步过滤)
         */
        template<class... Components>
        [[nodiscard]] auto view() {
            return View<Components...>(*this);
        }

        /**
         * @brief 为实体生成 EntityWrapper 句柄包装器
         */
        [[nodiscard]] EntityWrapper wrap(const Entity& e);

        /**
         * @brief 创建新实体并返回其 EntityWrapper 句柄包装器
         */
        [[nodiscard]] EntityWrapper create_wrapper();

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(
                std::back_inserter(target),
                "EntityManager(entities: {}, pools: {})",
                entities.occupied_count(), component_pools.size()
            );
        }
    };

    /**
     * @brief 实体句柄的面向对象式 RAII 包装器
     */
    class EntityWrapper {
    private:
        EntityManager* em{nullptr};
        Entity e{Entity::null()};

    public:
        EntityWrapper() = default;

        explicit EntityWrapper(EntityManager& manager)
            : em(&manager), e(manager.create_entity()) {}

        EntityWrapper(EntityManager& manager, const Entity& et)
            : em(&manager) {
            set_entity(et);
        }

        [[nodiscard]] Entity get_entity() const noexcept { return e; }
        [[nodiscard]] bool is_null() const noexcept { return e.is_null(); }
        [[nodiscard]] bool is_valid() const noexcept { return em && em->is_valid(e); }

        [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }
        [[nodiscard]] operator Entity() const noexcept { return e; }

        template<class T>
        [[nodiscard]] bool has() const noexcept {
            if (!em) return false;
            return em->has_component<T>(e);
        }

        template<class T>
        [[nodiscard]] T* get_raw() const noexcept {
            if (!em) return nullptr;
            return em->get_component_raw<T>(e);
        }

        template<class T>
        [[nodiscard]] std::optional<ref_t<T>> get() {
            panic_debug(!is_valid(), "EntityWrapper::get on invalid entity!");
            if (!em) return std::nullopt;
            return em->get_component<T>(e);
        }

        template<class T, class... Args>
        ref_t<T> add(Args&&... args) {
            panic_debug(!is_valid(), "EntityWrapper::add on invalid entity!");
            return em->add_component<T>(e, std::forward<Args>(args)...);
        }

        template<class... Cs>
        std::tuple<ref_t<Cs>...> adds() {
            panic_debug(!is_valid(), "EntityWrapper::adds on invalid entity!");
            return std::make_tuple(em->add_component<Cs>(e)...);
        }

        template<class T>
        void remove() {
            panic_debug(!is_valid(), "EntityWrapper::remove on invalid entity!");
            if (em) em->remove_component<T>(e);
        }

        void set_entity(const Entity& et) {
            if (!em) {
                e = Entity::null();
                return;
            }
            if (em->is_valid(et)) {
                e = et;
            } else {
                e = Entity::null();
            }
        }

        void destroy() {
            if (!is_valid()) return;
            em->destroy_entity(e);
            e = Entity::null();
        }

        template<class Target>
        void write_to_log(Target& target) const {
            if (is_valid()) {
                std::format_to(std::back_inserter(target), "EntityWrapper(id: {}, v: {})", e.id, e.version);
            } else {
                target.append("EntityWrapper(invalid)");
            }
        }
    };

    inline EntityWrapper EntityManager::wrap(const Entity& e) {
        return EntityWrapper(*this, e);
    }

    inline EntityWrapper EntityManager::create_wrapper() {
        return EntityWrapper(*this);
    }

    // ==================== View 模板方法具体实现 ====================

    template<class... Components>
    template<class F>
    void View<Components...>::for_each(F&& func) const {
        if constexpr (sizeof...(Components) == 0) return;
        if (!em) return;

        std::tuple<ComponentPool<Components>*...> pools = {
            em->template get_component_pool<Components>()...
        };

        bool all_exist = ((std::get<ComponentPool<Components>*>(pools) != nullptr) && ...);
        if (!all_exist) return;

        bool any_empty = ((std::get<ComponentPool<Components>*>(pools)->size() == 0) || ...);
        if (any_empty) return;

        ComponentPoolBase* min_pool = nullptr;
        usize min_size = std::numeric_limits<usize>::max();

        auto find_min = [&]<class C>(ComponentPool<C>* p) {
            if (p && p->size() < min_size) {
                min_size = p->size();
                min_pool = p;
            }
        };
        (find_min(std::get<ComponentPool<Components>*>(pools)), ...);

        if (!min_pool || min_size == 0) return;

        auto iterate_with_pool = [&]<class LeadC>(ComponentPool<LeadC>* lead_pool) {
            for (const auto& [entity_id, lead_slot] : lead_pool->mapper) {
                bool matches = ((std::get<ComponentPool<Components>*>(pools)->has(entity_id)) && ...);
                if (!matches) continue;

                Entity ent = em->get_entity_by_id(entity_id);
                if (ent.is_null()) continue;

                if constexpr (requires { func(ent, std::get<ComponentPool<Components>*>(pools)->data[*std::get<ComponentPool<Components>*>(pools)->try_get_slot(entity_id)]...); }) {
                    func(ent, std::get<ComponentPool<Components>*>(pools)->data[*std::get<ComponentPool<Components>*>(pools)->try_get_slot(entity_id)]...);
                } else if constexpr (requires { func(std::get<ComponentPool<Components>*>(pools)->data[*std::get<ComponentPool<Components>*>(pools)->try_get_slot(entity_id)]...); }) {
                    func(std::get<ComponentPool<Components>*>(pools)->data[*std::get<ComponentPool<Components>*>(pools)->try_get_slot(entity_id)]...);
                } else if constexpr (requires { func(ent, alib6::ref(std::get<ComponentPool<Components>*>(pools)->data, *std::get<ComponentPool<Components>*>(pools)->try_get_slot(entity_id))...); }) {
                    func(ent, alib6::ref(std::get<ComponentPool<Components>*>(pools)->data, *std::get<ComponentPool<Components>*>(pools)->try_get_slot(entity_id))...);
                }
            }
        };

        bool executed = false;
        auto try_lead = [&]<class C>(ComponentPool<C>* p) {
            if (!executed && static_cast<ComponentPoolBase*>(p) == min_pool) {
                iterate_with_pool.template operator()<C>(p);
                executed = true;
            }
        };
        (try_lead(std::get<ComponentPool<Components>*>(pools)), ...);
    }

    template<class... Components>
    void View<Components...>::Iterator::advance_to_valid() {
        while (current_it != end_it) {
            id_t eid = current_it->first;
            bool matches = (
                ((std::get<ComponentPool<Components>*>(pools) != nullptr) && ...) &&
                ((std::get<ComponentPool<Components>*>(pools)->has(eid)) && ...)
            );
            if (matches && em && em->is_valid(em->get_entity_by_id(eid))) {
                break;
            }
            ++current_it;
        }
    }

    template<class... Components>
    decltype(auto) View<Components...>::Iterator::operator*() const {
        id_t eid = current_it->first;
        Entity ent = em->get_entity_by_id(eid);
        return std::tuple<Entity, Components&...>(
            ent,
            std::get<ComponentPool<Components>*>(pools)->data[
                *std::get<ComponentPool<Components>*>(pools)->try_get_slot(eid)
            ]...
        );
    }

    template<class... Components>
    typename View<Components...>::Iterator View<Components...>::begin() const {
        if (!em) return end();
        std::tuple<ComponentPool<Components>*...> pools = {
            em->template get_component_pool<Components>()...
        };
        bool all_exist = ((std::get<ComponentPool<Components>*>(pools) != nullptr) && ...);
        if (!all_exist) return end();
        bool any_empty = ((std::get<ComponentPool<Components>*>(pools)->size() == 0) || ...);
        if (any_empty) return end();

        auto* lead_pool = std::get<0>(pools);
        return Iterator(em, pools, lead_pool->mapper.begin(), lead_pool->mapper.end());
    }

    template<class... Components>
    typename View<Components...>::Iterator View<Components...>::end() const {
        if (!em) return Iterator();
        std::tuple<ComponentPool<Components>*...> pools = {
            em->template get_component_pool<Components>()...
        };
        if (std::get<0>(pools) == nullptr) return Iterator();
        auto* lead_pool = std::get<0>(pools);
        return Iterator(em, pools, lead_pool->mapper.end(), lead_pool->mapper.end());
    }

    // ==================== 元组快速提取辅助函数 ====================

    template<class T, class... Ts>
    [[nodiscard]] inline ref_t<T> get(std::tuple<ref_t<Ts>...>&& t) {
        return std::move(std::get<ref_t<T>>(t));
    }

    template<class T, class... Ts>
    [[nodiscard]] inline ref_t<T>& get(std::tuple<ref_t<Ts>...>& t) {
        return std::get<ref_t<T>>(t);
    }

} // namespace alib6::ecs
