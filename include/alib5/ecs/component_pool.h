/**
 * @file component_pool.h
 * @brief Object pool storing components keyed by entity id. / 组件池，按 entity id 索引组件
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef AECS_COMPPOOL_INCLUDED
#define AECS_COMPPOOL_INCLUDED
#include <alib5/autil.h>
#include <alib5/ecs/entity.h>
#include <alib5/ecs/linear_storage.h>
#include <unordered_map>

namespace alib5::ecs{
    namespace detail{
        /**
         * @brief Type-erased destroyer base shared by every component pool instance.
         * @par Original Comment:
         * 每个池子对应的销毁基类
         */
        struct PoolDestroyerBase{
            /**
             * @brief Function pointer invoked with the owning pool pointer and the entity id.
             * @note  这里并不需要进行版本号管理，因为获取entity的时候不依赖版本号
             * @par Original Comment:
             * 销毁函数，第一个为pool的指针，第二个为对应的entity_id
             */
            int (*destroyer)(void*,id_t);
        };
    }

    /**
     * @brief Component pool combining LinearStorage with an id->slot map and a type-erased destroyer.
     * @tparam T 组件类型 / component type
     * @par Original Comment:
     * 组件池
     */
    template<class T> struct ALIB5_API ComponentPool : public detail::PoolDestroyerBase {
        /// @brief 具体的组件数据 / linear storage backing the components
        detail::LinearStorage<T> data;
        /// @brief 组件与 entity id 之间对应的映射 / mapping from entity id to component slot
        std::unordered_map<id_t,size_t> mapper;

        /**
         * @brief Initialize the component pool with an optional reservation size.
         * @param pool_reserve_size 预留大小 / reservation size for the underlying storage
         * @par Original Comment:
         * 初始化组件池
         */
        inline ComponentPool(size_t pool_reserve_size = 0)
        :data(pool_reserve_size){
            destroyer = nullptr;
        }
    };
}

#endif
