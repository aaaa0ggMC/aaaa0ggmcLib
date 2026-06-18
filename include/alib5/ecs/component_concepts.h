/**
 * @file component_concepts.h
 * @brief Concepts and injection helpers describing supported component injection styles for EntityManager. / entity_manager 支持的所有注入方式及辅助基类定义
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef AECS_COMPONENTS_CONCEPTS
#define AECS_COMPONENTS_CONCEPTS
#include <alib5/ecs/entity.h>

//// Component ////
namespace alib5::ecs{
    /**
     * @brief Concept requiring a cleanup() member used to release auxiliary data immediately when removing a component.
     * @par Original Comment:
     * 如果你的component中有需要在entity.remove_component<T>()时立马销毁的数据，可以通过写cleanup函数来实现
     * 参数要求:    无参数
     * 返回值要求：  无要求（因为不会被使用，因此建议为void）
     */
    template<class T> concept NeedCleanup = requires(T& t){t.cleanup();};
    /**
     * @brief Empty mixin marker indicating that a derived component requires cleanup.
     * @note  由于并不需要转换成基类来访问，因此其实没有vtable开销
     * @par Original Comment:
     * 可以通过继承这个基类来方便地表示你需要cleanup
     */
    struct INeedCleanup{
        // void cleanup();
    };

    /**
     * @brief Concept requiring a reset(Args...) member used to re-initialize a component when reusing a free slot.
     * @par Original Comment:
     * 添加component的时候要是不存在空的槽位会执行构造函数，参数传递
     * 比如e.add<T>(...) -> emplace<T>(...)，而要是有空的槽位会进行一个判断:
     * 如果存在reset函数，优先调用reset，否则执行析构再原位使用构造函数构造
     * 因此建议reset函数的签名和构造函数一致
     * 参数要求:   任意，但是要和传入的符合
     * 返回值要求： 无要求，不会被使用
     */
    template<class T,class... Args> concept NeedReset =
        requires(T&t,Args&&... args){t.reset(std::forward<Args>(args)...);};
    /**
     * @brief Empty mixin marker indicating that a derived component requires reset (empty-argument form only).
     * @par Original Comment:
     * 可以通过继承这个类来表示你需要reset，但是这个的继承只有空参列表的，用处不大
     */
    struct INeedReset{
        // void reset();
    };

    /**
     * @brief Concept requiring a d_cleanup() member invoked right after LinearStorage removes an element.
     * @par Original Comment:
     * LinearStorage删除元素后立马执行的操作
     * 参数要求:   空
     * 返回值要求： 无要求，不会被使用
     */
    template<class T> concept NeedDataCleanup =
        requires(T&t){t.d_cleanup();};
    /**
     * @brief Empty mixin marker indicating that a derived component requires d_cleanup (empty-argument form only).
     * @par Original Comment:
     * 可以通过继承这个类来表示你需要d_cleanup，但是这个的继承只有空参列表的，用处不大
     */
    struct INeedDataCleanup{
        // void d_cleanup();
    };

    /**
     * @brief Concept requiring bind(Entity) so the EntityManager binds the component to its owning Entity (or null on removal).
     * @par Original Comment:
     * 如果你希望在System中能够访问到组件目前绑定的entity，可以使用下面的注入方式
     * 当存在bind接口的时候，entity_manager会保证在创建组件的时候bind到对应的entity
     * 然后在删除组件的时候bind到空的entity(Entity::null())
     * 绑定具体值穿入的是左值，绑定空值传入的是右值
     * 如果cleanup和bind同时存在，先cleanup在bind空值
     * 参数要求: 第一个为entity
     * 返回值要求: 无要求，不会被使用
     */
    template<class T> concept NeedBind = requires(T&t,Entity e){t.bind(e);};
    /**
     * @brief Helper struct implementing simple entity binding storage and accessor.
     * @par Original Comment:
     * 通过下面进行简单注入
     */
    struct IBindEntity{
        /// @brief 当前绑定的 entity / bound entity
        Entity bound_entity;

        /**
         * @brief Bind the stored entity to the given reference.
         * @param e 引用的实体 / entity to bind
         */
        inline void bind(const Entity & e){
            bound_entity = e;
        }

        /**
         * @brief Return a const reference to the currently bound entity.
         * @return 已绑定的 entity / const reference to bound entity
         */
        inline const Entity& get_bound(){
            return bound_entity;
        }
    };

    /**
     * @brief Concept requiring slot(size_t) so the EntityManager informs the component of its slot index once at creation time.
     * @par Original Comment:
     * 如果你希望获取组件目前的slot可以设置这个，由于组件slot位置都不会变动，因此每次创建component的时候会bind
     * 复用的时候不会处理
     */
    template<class T> concept NeedSlotId = requires(T& t,size_t index){t.slot(index);};
    /**
     * @brief Helper struct implementing simple slot id injection storage and accessor.
     * @par Original Comment:
     * 简单注入
     */
    struct ISlotComponent{
        /// @brief 组件对应的 slot 索引 / stored slot index
        size_t m_slot;

        /**
         * @brief Store the provided slot index.
         * @param i slot 序号 / slot index
         */
        inline void slot(size_t i){
            m_slot = i;
        }

        /**
         * @brief Return the stored slot index.
         * @return 当前 slot / current slot index
         */
        inline size_t get_slot(){
            return m_slot;
        }
    };

    /**
     * @brief Concept requiring bind_dep(Tup) so the EntityManager can inject the dependency tuple into the component.
     * @par Original Comment:
     * 如果你希望获取你需要注入的依赖，可以使用bind_dep方法来设置，传入的是tuple
     * 需要Dependency::deptup_t 来获取，所以你的Dependency的类型要为
     * ComponentStack
     */
    template<class T,class Tup> concept NeedBindDependency = requires(T & t,Tup & tup){
        t.bind_dep(tup);
    };

    /**
     * @brief Concept requiring the component to declare a Dependency alias (typically ComponentStack<...>).
     * @par Original Comment:
     * 这里可以对组件进行依赖
     * 要求组件内存在 using Dependency = ComponentStack<...>;
     * 碰到循环依赖会报错
     * eg.
     * struct Comp{
     *  using Dependency = alib::g3::ecs::ComponentStack<Velocity,Mass,RigidBody>;
     * };
     */
    template<class T> concept NeedDependency = requires{typename T::Dependency;};

    /**
     * @brief Concept requiring update(Args...) so EntityManager::update<T> can dispatch updates uniformly.
     * @par Original Comment:
     * 如果你希望使用entitymanager.update<T>这个原始人函数来统一update你的组件，可以使用这个
     */
    template<class T,class... Args> concept NeedUpdate = requires(T & t,Args&&... t_args){t.update(t_args...);};

    /**
     * @brief Trait aggregator that statically detects which optional behaviors a component supports.
     * @par Original Comment:
     * 这个是用来检测你的某个类是否达到标准从而拥有某个trait的
     */
    template<class T> struct ComponentTraits{
        /// @brief 是否声明了 Dependency / whether the component declares a Dependency
        constexpr static bool dependency = NeedDependency<T>;

        /**
         * @brief Internal helper determining whether bind_dependency is actually usable given Dependency.
         * @return 当存在 Dependency 且支持 bind_dep 时为 true / true when Dependency exists and bind_dep is usable
         */
        constexpr static bool __get_bind_dependency(){
            if constexpr(dependency){
                if constexpr(NeedBindDependency<T,typename T::Dependency::deptup_t>){
                    return true;
                }
                return false;
            }
            return false;
        }

        /// @brief 是否支持 bind_dep 注入 / whether bind_dependency injection is supported
        constexpr static bool bind_dependency = __get_bind_dependency();
        /// @brief 是否支持 cleanup / whether cleanup is supported
        constexpr static bool cleanup = NeedCleanup<T>;
        /// @brief 是否支持 bind(Entity) / whether bind(Entity) is supported
        constexpr static bool bind = NeedBind<T>;
        /// @brief 是否支持 slot(size_t) / whether slot(size_t) is supported
        constexpr static bool slot_id = NeedSlotId<T>;
        /// @brief 是否支持 reset(Args...) / whether reset(Args...) is supported
        template<class... Args> constexpr static bool reset = NeedReset<T,Args...>;
        /// @brief 是否支持 update(Args...) / whether update(Args...) is supported
        template<class... Args> constexpr static bool update = NeedUpdate<T,Args...>;

        /**
         * @brief Format a human-readable trait summary into the given target using std::format_to.
         * @tparam Tg 目标容器类型 / target container type
         * @param target 接收格式化输出的容器 / target receiving formatted output
         */
        template<class Tg> static void write_to_log(Tg & target){
            auto simp_yn = [](bool val){
                return val?"Y":"N";
            };

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
                simp_yn(dependency),
                simp_yn(cleanup),
                simp_yn(bind),
                simp_yn(slot_id),
                simp_yn(bind_dependency),
                simp_yn(requires(T && t){t.reset();}),
                simp_yn(requires(T && t){t.update();})
            );
        }

        /**
         * @brief Stream a human-readable trait summary into the given target with a trailing token.
         * @tparam Target 输出流类型 / output stream type
         * @tparam EndToken 终止 token 类型 / end token type
         * @param t 输出流 / output stream
         * @param end_token 写入结束后的 token / token appended after the summary
         */
        template<class Target,class EndToken> static void check(Target && t,EndToken && end_token){
            auto simp_yn = [](bool val){
                return val?"Y":"N";
            };

            std::forward<Target>(t) << typeid(T).name() << ":\n"
            "\tDependency            :" << simp_yn(dependency) << "\n"
            "\tCleanup               :" << simp_yn(cleanup) << "\n"
            "\tBindEntity            :" << simp_yn(bind) << "\n"
            "\tBindSlotId            :" << simp_yn(slot_id) << "\n"
            "\tBindDependency        :" << simp_yn(bind_dependency) << "\n"
            "\tHasEmptyReset         :" << simp_yn(requires(T && t){t.reset();}) << "\n"
            "\tHasEmptyUpdate        :" << simp_yn(requires(T && t){t.update();})
            << std::forward<EndToken>(end_token);
        }
    };
}
#endif
