/**
 * @file cycle_checker.h
 * @brief Static detection of cyclic dependencies among component types. / 对循环依赖的组件进行检测
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef AECS_CYCLE_CHECKER
#define AECS_CYCLE_CHECKER
#include <alib5/aref.h>
#include <alib5/ecs/linear_storage.h>
#include <type_traits>

namespace alib5::ecs{
    namespace detail{
        /**
         * @brief Primary template searching Ts... for a duplicated Compare type.
         * @tparam Compare 比较的内容 / type being compared
         * @tparam ...Ts 目前已经存在的类型列表 / accumulated type list so far
         * @tparam Already 是否找到了第一个类型 / whether the first occurrence has already been seen
         * @par Original Comment:
         * 基类，查找Ts...中是否出现两个Compare
         */
        template<bool Already,class Compare,class... Ts> struct CycleChecker;

        /**
         * @brief Recursive specialization stepping through one type at a time accumulating conflict state.
         * @par Original Comment:
         * 基础特化，用于构造递归链条
         */
        template<bool Already,class Compare,class T,class... Ts>
            struct CycleChecker<Already,Compare,T,Ts...>{
            /// @brief 当前类型是否匹配 Compare / whether the current type matches Compare
            constexpr static bool matched = std::is_same_v<Compare,T>;
            /// @brief 递归到下一层的 Already 标志 / Already flag propagated to the next level
            constexpr static bool next_already = Already || matched;
            /// @brief 当前或后续是否出现冲突 / true if a conflict is detected at this level or any deeper level
            constexpr static bool conflict = (Already&&matched) || CycleChecker<next_already,Ts...>::conflict;
        };

        /**
         * @brief Terminal specialization handling the last type in the list.
         * @par Original Comment:
         * 终止递归
         */
        template<bool Already,class Compare,class T>
            struct CycleChecker<Already,Compare,T>{
            /// @brief 当前类型是否匹配 Compare / whether the current type matches Compare
            constexpr static bool matched = std::is_same_v<Compare,T>;
            /// @brief 递归到下一层的 Already 标志 / Already flag propagated to the next level
            constexpr static bool next_already = Already || matched;
            /// @brief 末位是否出现冲突 / true if a conflict is detected at this terminal level
            constexpr static bool conflict = (Already&&matched);
        };

        /**
         * @brief Empty-list specialization that never reports a conflict.
         * @par Original Comment:
         * 用于处理异常情况
         */
        template<bool Already,class Compare>
            struct CycleChecker<Already,Compare>{
            /// @brief 空列表无冲突 / no conflict possible on an empty list
            constexpr static bool conflict = false;
        };
    };


    /**
     * @brief Alias producing a safe RefWrapper around LinearStorage<T>.
     * @par Original Comment:
     * 获取对应的安全引用
     */
    template<class T> using ref_t = RefWrapper<detail::LinearStorage<T>>;
    /**
     * @brief Recursive type list used to track the dependency chain and detect cycles.
     * @tparam ...Ts 类型列表 / accumulated type list
     * @par Original Comment:
     * 用于递归的传入链，也是Dependency的依赖链
     */
    template<class... Ts> struct ComponentStack{
        /**
         * @brief Build a new ComponentStack with T prepended.
         * @par Original Comment:
         * 添加一个新的类型
         */
        template<class T> using add_t = ComponentStack<T,Ts...>;
        /// @brief 由 ref_t 包裹的 tuple 类型 / tuple type wrapping each dependency with ref_t
        using deptup_t = std::tuple<ref_t<Ts>...>;

        /**
         * @brief Statically check whether T already appears in the dependency chain.
         * @tparam T 要检测的类型 / type to check
         * @return 出现循环依赖返回 true / true when a cycle would be introduced
         * @par Original Comment:
         * 检查是否出现循环依赖
         */
        template<class T> constexpr inline static bool check_cycle(){
            return detail::CycleChecker<false,T,Ts...>::conflict;
        }
    };
}
#endif
