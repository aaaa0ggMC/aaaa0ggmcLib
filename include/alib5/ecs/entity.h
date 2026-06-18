/**
 * @file entity.h
 * @brief Minimal entity value type carrying an id and a version counter. / 基础的实体
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef AECS_ENTITY_INCLUDED
#define AECS_ENTITY_INCLUDED
#include <alib5/autil.h>

namespace alib5::ecs{
    /**
     * @brief Type used to identify an entity within a manager.
     * @par Original Comment:
     * 实体id的类型
     */
    using id_t = uint64_t;

    /**
     * @brief Entity value composed of an id and a version counter.
     * @par Original Comment:
     * 实体类
     */
    struct ALIB5_API Entity{
        /// @brief 实体的 id / entity id
        id_t id;
        /// @brief 实体的版本 / entity version
        uint32_t version;

        /**
         * @brief Default-construct an empty entity with id and version set to zero.
         * @par Original Comment:
         * 默认的实体构造
         */
        Entity(){
            id = version = 0;
        }

        /**
         * @brief Construct an entity with the given id and optional version.
         * @param i_id 实体 id / entity id
         * @param i_version 实体版本 / entity version
         * @par Original Comment:
         * 包含id以及版本的构造
         */
        Entity(id_t i_id,uint32_t i_version = 0){
            id = i_id;
            version = i_version;
        }

        /**
         * @brief Reuse the entity slot by bumping the version counter.
         * @par Original Comment:
         * 复用实体，这个会增加版本号
         */
        void reset(){
            version++;
        }

        /**
         * @brief Semantically explicit factory for an empty/null entity.
         * @return 空实体 / null entity
         * @par Original Comment:
         * 语义区分创建一个空的实体
         */
        static inline Entity null(){
            return Entity();
        }
    };

    //// 一些alias ////
    using entity = alib5::ecs::Entity;
    using entity_t = alib5::ecs::Entity;
}

#endif
