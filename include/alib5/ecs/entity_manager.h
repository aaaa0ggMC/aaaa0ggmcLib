/**
 * @file entity_manager.h
 * @brief Entity manager maintaining entities, component pools and dependency injection. / 实体管理
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef ALIB_EM_H_INCLUDED
#define ALIB_EM_H_INCLUDED
#include <alib5/ecs/entity.h>
#include <alib5/ecs/component_pool.h>
#include <alib5/ecs/linear_storage.h>
#include <alib5/ecs/cycle_checker.h>
#include <alib5/ecs/component_concepts.h>
#include <alib5/aref.h>
#include <alib5/adebug.h>
#include <memory>
#include <tuple>

namespace alib5::ecs{
    /**
     * @brief Entity manager owning component pools and a linear entity store with slot reuse.
     * @start-date 2025/11/27
     * @par Original Comment:
     * 实体管理器
     */
    struct ALIB5_API EntityManager{
    private:
        friend class EntityWrapper;
        /// @brief 组件池 / per-type component pools keyed by type hash
        std::unordered_map<uint64_t,std::unique_ptr<void,void(*)(void*)>> component_pool;
        /// @brief 存储实体的线性存储池 / linear storage of entities
        detail::LinearStorage<Entity> entities;
        /// @brief 最大的id,id从1开始 / maximum allocated id, ids start at 1
        id_t id_max;
        /// @brief 创建新的组件池子的时候预留的组件数量 / reservation size for newly created pools
        size_t pool_reserve_size;

        /**
         * @brief Look up the component pool for type T without bounds checks.
         * @return 组件池，找不到返回nullptr / pool pointer or nullptr if missing
         * @par Original Comment:
         * 获取对应类型的对应组件池
         */
        template<class T> ComponentPool<T>* get_component_pool_unsafe(){
            auto it = component_pool.find(typeid(T).hash_code());
            if(it == component_pool.end())return nullptr;
            return (ComponentPool<T>*)it->second.get();
        }

        /**
         * @brief Deleter used by the unique_ptr holding a type-erased component pool.
         * @par Original Comment:
         * unique_ptr需要一个deleter
         */
        template<class T> inline static void component_pool_destroyer(void* ptr){
            delete static_cast<T*>(ptr);
        }

        /**
         * @brief Resolve the component pool and slot index for the given entity.
         * @param e 对应的entity / owning entity
         * @param[out] index component的索引 / output slot index
         * @param[out] p 对应组件池的类型，找不到组件池返回nullptr，找不到component则设置索引为size_t::max / output pool pointer
         * @par Original Comment:
         * 获取对应component
         */
        template<class T> void get_component_impl(const Entity & e,size_t& index,ComponentPool<T>* &p){
            p = get_component_pool_unsafe<T>();
            if(!p)return;
            auto it = p->mapper.find(e.id);
            if(it == p->mapper.end()){
                index = std::numeric_limits<size_t>::max();
            }else{
                index = it->second;
            }
        }
    public:
        /// @brief 获取对应的安全引用 / alias for a safe reference to a component of type T
        template<class T> using ref_t = alib5::ecs::ref_t<T>;

        /// @brief 获取当前实体池子的大小 / return the number of slots in the entity pool
        inline size_t get_entity_pool_size(){
            return entities.size();
        }

        /**
         * @brief Create (or reuse) a component pool for type T and return its pointer.
         * @return 组件池的指针，保证不为nullptr / non-null pool pointer
         * @par Original Comment:
         * 添加一个新的组件池
         */
        template<class T> inline ComponentPool<T>* add_component_pool(){
            auto hash_code = typeid(T).hash_code();
            auto it = component_pool.find(hash_code);

            if(it != component_pool.end()){
                return (ComponentPool<T>*)it->second.get();
            }
            auto iter = component_pool.emplace(hash_code,
                std::unique_ptr<void,void(*)(void*)>(
                    (void*)(new ComponentPool<T>(pool_reserve_size)),
                    &(EntityManager::component_pool_destroyer<ComponentPool<T>>)
                )
            );
            ComponentPool<T> * pool = (ComponentPool<T>*)iter.first->second.get();
            pool->destroyer = [](void * pobj,id_t entity_id)->int{
                ComponentPool<T> & obj = *(ComponentPool<T>*)(pobj);
                auto it = obj.mapper.find(entity_id);
                if(it == obj.mapper.end())return -1;
                if constexpr(ComponentTraits<T>::cleanup){
                    // 你是有非销毁不可的理由吗？
                    obj.data.data[it->second].cleanup();
                }
                if constexpr(ComponentTraits<T>::bind){
                    obj.data.data[it->second].bind(Entity::null());
                }
                obj.data.remove(it->second);
                obj.mapper.erase(it);
                return 0;
            };
            return pool;
        }

        /**
         * @brief Ensure a component pool exists and return a reference to it.
         * @tparam T 组件类型 / component type
         * @return 组件池的指针，保证不为nullptr / non-null pool reference
         * @par Original Comment:
         * 确保并获取对应类型的组件池
         */
        template<class T> inline ComponentPool<T>& get_or_create_pool(){
            // 实际调用原来的 add_component_pool 或将实现移动到这里
            return *add_component_pool<T>();
        }

        /**
         * @brief Look up the component pool for type T.
         * @return 组件池，找不到返回nullptr / pool pointer or nullptr if missing
         * @par Original Comment:
         * 获取对应类型的对应组件池
         */
        template<class T> inline ComponentPool<T>* get_component_pool(){
            return get_component_pool_unsafe<T>();
        }

        /// @brief 获取持有的entity线性存储 / access the underlying entity storage
        inline auto& get_entities_storage(){
            return entities;
        }

        /**
         * @brief Construct an EntityManager with optional entity and pool reservation sizes.
         * @param entity_reserve_size 实体预留数量 / reservation for the entity storage
         * @param pool_res 池子预留数量 / reservation applied to each new component pool
         * @par Original Comment:
         * 构造实体表
         */
        inline EntityManager(size_t entity_reserve_size = 0,size_t pool_res = 0)
        :entities(entity_reserve_size){
            pool_reserve_size = pool_res;
            id_max = 0;
        }

        /**
         * @brief Create a new entity, reusing a free slot when one is available.
         * @return 实体 / newly created entity
         * @par Original Comment:
         * 创建一个新的实体
         */
        inline Entity create_entity(){
            if(entities.free_elements.empty()){
                // 从这里可以看到entities的id是和LinearStorage的index基本对齐的 index = id - 1
                return entities.next(++id_max);
            }else{
                return entities.next_free();
            }
        }

        /**
         * @brief Destroy an entity and remove it from every component pool.
         * @param e 实体，只看id / entity (only id is consulted)
         * @par Original Comment:
         * 销毁实体
         */
        inline void destroy_entity(const Entity & e){
            for(auto & ref_comp : component_pool){
                //@Note: its data is not usable!!!
                auto cp = (ref_comp.second.get());
                auto destroyer = ((detail::PoolDestroyerBase*)cp)->destroyer;
                // 孩子们，我可能会崩溃吗，应该不可能吧
                destroyer(cp,e.id);
            }
            entities.remove(e.id - 1);
        }

        /**
         * @brief Get a raw pointer to the component of type T attached to entity e.
         * @param e 实体 / owning entity
         * @return 指针，nullptr表示没找到 / pointer or nullptr if not found
         * @par Original Comment:
         * 获取实体原始的组件数据
         */
        template<class T> T* get_component_raw(const Entity & e){
            size_t index;
            ComponentPool<T> * p;
            get_component_impl<T>(e,index,p);
            if(p && index != std::numeric_limits<size_t>::max())return &((*p).data.data[index]);
            else return nullptr;
        }

        /**
         * @brief Get a safe reference to the component of type T attached to entity e.
         * @param e 实体 / owning entity
         * @return nullopt表示没有对应组件，否则返回组件的安全引用 / optional safe reference
         * @par Original Comment:
         * 获取对应实体对应组件的安全引用
         */
        template<class T> std::optional<ref_t<T>> get_component(const Entity & e){
            size_t index;
            ComponentPool<T> * p;
            get_component_impl<T>(e,index,p);
            if(p && index >= 0)return ref((p->data),index);
            else return std::nullopt;
        }

        /**
         * @brief Attach a new component of type T to entity e, handling dependency injection and cycle detection.
         * @tparam T 添加的类型 / component type to add
         * @tparam Tuo 目前的组件链条 / current dependency chain
         * @param e 对应的实体 / owning entity
         * @param ...args 组件T的构造函数，遗憾的是依赖的类型只能使用空参数的构造函数 / constructor arguments for T
         * @return 对应的安全引用，语义上保证非空 / non-null safe reference
         * @par Original Comment:
         * 添加一个新的组件，支持依赖也进行添加，会识别循环依赖
         */
        template<class T,class Tuo = ComponentStack<> ,class... Args> EntityManager::ref_t<T> add_component(const Entity & e,Args&&... args){
            ComponentPool<T> * p = add_component_pool<T>();
            auto it = p->mapper.find(e.id);
            if(it != p->mapper.end())return ref(p->data,it->second);

            auto create = [&](auto&& deps){
                bool flag;
                size_t index;
                T & comp = p->data.try_next_with_index(flag,index,std::forward<Args>(args)...);
                if constexpr(ComponentTraits<T>::bind){
                    comp.bind(e);
                }
                /// 为了让clangd不报错，我只能这么写了。。。
                if constexpr(ComponentTraits<T>::dependency){
                    if constexpr(ComponentTraits<T>::bind_dependency){
                        comp.bind_dep(deps);
                    }
                }
                if constexpr(ComponentTraits<T>::slot_id){
                    if(flag){
                        comp.slot(index);
                    }
                }
                p->mapper.emplace(e.id,index);
                return ref(p->data,index);
            };

            // 创建新的component
            // 依赖
            // 就算没有依赖也不得不创建一个tuple，虽然数据都是简易类型，还是有点亏
            if constexpr(ComponentTraits<T>::dependency){
                using Tuo_Next = typename Tuo::template add_t<T>;
                // 检测循环依赖
                static_assert(!Tuo_Next::template check_cycle<T>(),"Cycle dependency!");

                typename T::Dependency::deptup_t deps;
                deps = []<class... TArgs>(EntityManager & em,const Entity & e,ComponentStack<TArgs...>){
                    return std::make_tuple(
                        (em.add_component<TArgs,Tuo_Next>(e))
                    ...);
                }(*this,e,typename T::Dependency{});

                return create(deps);
            }else{
                // 具体创建
                return create(std::ignore);
            }
        }

        /**
         * @brief Result codes returned by remove_component.
         * @par Original Comment:
         * 移除组件的返回值
         */
        enum DestroyResult{
            DRSuccess  = 0, ///< 成功删除组件 / component removed successfully
            DRNoPool   = 1, ///< 没有对应的组件池 / no pool for this component type
            DRCantFind = 2  ///< 组件池找不到对应entity的mapper / entity not present in the pool
        };

        /**
         * @brief Remove the component of type T attached to entity e.
         * @tparam T 组件的类型 / component type
         * @param e 实体 / owning entity
         * @return 移除组件的结果 / result code
         * @par Original Comment:
         * 移除对应的component
         */
        template<class T> DestroyResult remove_component(const Entity & e){
            ComponentPool<T> * p = get_component_pool_unsafe<T>();
            if(!p)return DRNoPool;
            if(p->destroyer((void*)p,e.id) == -1){
                return DRCantFind;
            }

            return DRSuccess;
        }

        /**
         * @brief Iterate over occupied components of type T invoking f for each.
         * @tparam T 组件池类型 / component type
         * @param f 函数类型 / functor invoked with each component reference
         * @par Original Comment:
         * 更新所有的非空闲的组件
         */
        template<class T,detail::FuncForEachable<T> F> inline void update(F && f){
            ComponentPool<T> * pool = get_component_pool<T>();
            if(pool){
                pool->data.for_each(std::forward<F>(f));
            }
        }


        /**
         * @brief Dispatch update(Args...) to every occupied component of type T.
         * @tparam T 组件池类型，组件内需要有update函数 / component type exposing an update member
         * @param Args update 参数 / arguments forwarded to each component's update
         * @par Original Comment:
         * 更新所有的非空闲的组件
         */
        template<class T,class... Args> inline void update(Args&&... args){
            static_assert(ComponentTraits<T>::template update<Args...>,"There is not update function in the component.");
            ComponentPool<T> * pool = get_component_pool_unsafe<T>();
            if(!pool)return;

            auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

            pool->data.available_bits.for_each_skip_1_bits(
                [pool,args_tuple = std::move(args_tuple)](size_t index) mutable{
                    std::apply(
                        [&](auto&&... func_args){
                            pool->data.data[index].update(std::forward<decltype(func_args)>(func_args)...);
                        },
                        args_tuple
                    );
                },pool->data.data.size()
            );
        }
    };

    /**
     * @brief RAII wrapper around an EntityManager-owned entity providing component accessors.
     * @par Original Comment:
     * 实体的wrapper
     */
    struct ALIB5_API EntityWrapper{
    private:
        /// @brief 实体管理 / owning EntityManager
        EntityManager & em;
        /// @brief 实体 / wrapped entity
        Entity e;
    public:
        /// @brief 获取实体 / return the wrapped entity
        Entity get_entity(){
            return e;
        }

        /**
         * @brief Construct a wrapper that creates a fresh entity in the given manager.
         * @param manager 对应的manager / owning EntityManager
         * @par Original Comment:
         * 实体的wrapper
         */
        EntityWrapper(EntityManager & manager):em(manager){
            e = manager.create_entity();
        }

        /**
         * @brief Wrap an existing entity, validating it via set_entity.
         * @param et 实体引用 / entity to wrap
         * @par Original Comment:
         * 从已经存在的实体进行包装吗，其实会更新版本号码
         */
        EntityWrapper(EntityManager & manager,const Entity & et):em(manager){
            set_entity(et);
        }

        /// @brief 检查当前的实体是否为空 / return true when the wrapped entity is null
        bool is_null(){
            return e.id == 0;
        }

        /**
         * @brief Get a safe reference to the component of type T attached to the wrapped entity.
         * @return nullopt表示找不到组件 / optional safe reference
         * @par Original Comment:
         * 获取对应的组件
         */
        template<class T> std::optional<EntityManager::ref_t<T>> get(){
            panic_debug(e.id == 0,"Invalid handle.");
            return em.get_component<T>(e);
        }

        /**
         * @brief Attach a new component of type T to the wrapped entity.
         * @return 组件的安全引用 / safe reference to the new component
         * @par Original Comment:
         * 添加一个新的组件
         */
        template<class T,class... Args> EntityManager::ref_t<T> add(Args&&... args){
            panic_debug(e.id == 0,"Invalid handle.");
            return em.add_component<T>(e,std::forward<Args>(args)...);
        }

        /**
         * @brief Attach multiple components at once.
         * @tparam ...Cs 组件类型 / component types
         * @return 组件安全引用的tuple，无法通过std::get<基础类型获取>，可以通过alib::g3::ecs::get<基础类型>获取 / tuple of safe references
         * @par Original Comment:
         * 添加若干组件
         */
        template<class... Cs> std::tuple<EntityManager::ref_t<Cs>...> adds(){
            panic_debug(e.id == 0,"Invalid handle.");
            return std::make_tuple(
                em.add_component<Cs>(e)...
            );
        }

        /**
         * @brief Remove the component of type T from the wrapped entity.
         * @tparam T 类型 / component type
         * @par Original Comment:
         * 移除组件，由于复杂度原因不支持反向依赖删除组件
         */
        template<class T> void remove(){
            panic_debug(e.id == 0,"Invalid handle.");
            if(e.id)em.remove_component<T>(e);
        }

        /**
         * @brief Rebind the wrapper to the given entity after validation.
         * @param et 实体 / entity to bind
         * @par Original Comment:
         * 设置entity，会检测entity是否有效（null&valid&version check)
         */
        void set_entity(const Entity & et){
            if(!et.id || et.id > em.entities.size()){
                e = Entity::null();
                return;
            }
            if(!em.entities.available_bits.get(et.id - 1)){
                e = Entity::null();
            }else{
                Entity & latest = em.entities[et.id - 1];
                if(latest.version != et.version){
                    e = Entity::null();
                }else e = latest;
            }
        }

        /// @brief 删除实体 / destroy the wrapped entity
        void destroy(){
            panic_debug(e.id == 0,"Invalid handle.");
            if(e.id == 0)return;

            em.destroy_entity(e);
            e = Entity::null();
        }
    };

    /**
     * @brief Extract a safe reference of type T from an rvalue tuple returned by adds().
     * @tparam T 类型 / target component type
     * @tparam ...Ts 返回的tuple中的类型表，为RefWrapper<X>... / tuple element types
     * @param t 对应的右值tuple / rvalue tuple
     * @return 对应的安全引用 / extracted safe reference
     * @par Original Comment:
     * 获取adds返回的tuple对应位置的引用
     */
    template<class T,class... Ts> inline EntityManager::ref_t<T>
        get(std::tuple<EntityManager::ref_t<Ts>...> && t){
            return std::move(std::get<EntityManager::ref_t<T>>(t));
    }

    /**
     * @brief Extract a safe reference of type T from an lvalue tuple returned by adds().
     * @tparam T 类型 / target component type
     * @tparam ...Ts 返回的tuple中的类型表，为RefWrapper<X>... / tuple element types
     * @param t 对应的左值tuple / lvalue tuple
     * @return 对应的安全引用 / extracted safe reference
     * @par Original Comment:
     * 获取adds返回的tuple对应位置的引用
     */
    template<class T,class... Ts> inline EntityManager::ref_t<T>&
        get(std::tuple<EntityManager::ref_t<Ts>...> & t){
            return std::get<EntityManager::ref_t<T>>(t);
    }

    /// @brief 简易包装 / convenience alias for EntityManager::ref_t
    template<class T> using ref_t = EntityManager::ref_t<T>;
}

#endif
