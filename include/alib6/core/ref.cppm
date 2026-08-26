/**
 * @file ref.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 防悬垂安全容器索引引用包装器 (支持单层与多层嵌套访问，操作符完全穿透)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>

export module alib6.core:ref;
import std;
import :types;
import :debug;
import :concepts;

export namespace alib6 {

    /**
     * @brief 判定是否为支持随机索引、大小查询的容器概念
     */
    template<class T>
    concept IndexableContainer = requires(T& t, usize i) {
        t[i];
        t.size();
        t.empty();
        typename T::value_type;
    };

    /**
     * @brief 单层容器安全索引引用包装器
     * 通过持有容器指针与元素索引，保证容器扩容/移动时不发生原生指针悬垂。
     * 所有 operator（包括 [] 与 ()）均对底层被引用的元素实现 100% 透明穿透。
     * 索引与内存偏移导航一律采用显式命名方法（如 next_element()）。
     */
    template<IndexableContainer Cont>
    struct RefWrapper {
        using BaseType = typename Cont::value_type;
        using ReferenceType = decltype(std::declval<Cont&>()[0]);
        using ConstReferenceType = decltype(std::declval<const Cont&>()[0]);

        Cont* cont{nullptr};
        usize index{std::numeric_limits<usize>::max()};

        [[nodiscard]] constexpr bool has_data() const noexcept {
            return cont && index < cont->size();
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            return has_data();
        }

        [[nodiscard]] constexpr decltype(auto) get() {
            panic_if(!cont, "RefWrapper: root container pointer is null!");
            panic_if(index >= cont->size(), "RefWrapper: index out of bounds!");
            return (*cont)[index];
        }

        [[nodiscard]] constexpr decltype(auto) get() const {
            panic_if(!cont, "RefWrapper: root container pointer is null!");
            panic_if(index >= cont->size(), "RefWrapper: index out of bounds!");
            return (*cont)[index];
        }

        [[nodiscard]] constexpr auto* ptr() {
            return std::addressof(get());
        }

        [[nodiscard]] constexpr const auto* ptr() const {
            return std::addressof(get());
        }

        // ==================== 解引用与原生引用穿透 ====================
        constexpr decltype(auto) operator*() { return get(); }
        constexpr decltype(auto) operator*() const { return get(); }

        constexpr auto* operator->() { return ptr(); }
        constexpr const auto* operator->() const { return ptr(); }

        constexpr operator ReferenceType() { return get(); }
        constexpr operator ConstReferenceType() const { return get(); }

        // ==================== 下标 [] 与调用 () 透明穿透 ====================
        template<class... Args>
        constexpr decltype(auto) operator[](Args&&... args)
            requires requires { get()[std::forward<Args>(args)...]; }
        {
            return get()[std::forward<Args>(args)...];
        }

        template<class... Args>
        constexpr decltype(auto) operator[](Args&&... args) const
            requires requires { get()[std::forward<Args>(args)...]; }
        {
            return get()[std::forward<Args>(args)...];
        }

        template<class... Args>
        constexpr decltype(auto) operator()(Args&&... args)
            requires requires { get()(std::forward<Args>(args)...); }
        {
            return get()(std::forward<Args>(args)...);
        }

        template<class... Args>
        constexpr decltype(auto) operator()(Args&&... args) const
            requires requires { get()(std::forward<Args>(args)...); }
        {
            return get()(std::forward<Args>(args)...);
        }

        // ==================== 显式索引偏移与导航 API ====================
        constexpr RefWrapper& next_element() { return set_index(index + 1); }
        constexpr RefWrapper& prev_element() { return set_index(index - 1); }
        constexpr RefWrapper& advance(isize offset) { return set_index(static_cast<usize>(static_cast<isize>(index) + offset)); }

        [[nodiscard]] constexpr RefWrapper offset_element(isize offset) const {
            auto t = *this;
            t.set_index(static_cast<usize>(static_cast<isize>(index) + offset));
            return t;
        }

        constexpr RefWrapper& set_index(usize idx) {
            if (!cont) panic("RefWrapper: root container pointer is null!");
            if (idx >= cont->size()) panicf("RefWrapper: index out of bounds! (index: {}, size: {})", idx, cont->size());
            this->index = idx;
            return *this;
        }

        [[nodiscard]] constexpr usize get_index() const noexcept { return index; }

        // ==================== 赋值、算术与比较操作符穿透 ====================
        #define ALIB6_DEFINE_REF_OP(op) \
            template<class U> \
            constexpr auto operator op(U&& val) const \
                requires requires { get() op std::forward<U>(val); } \
            { \
                return get() op std::forward<U>(val); \
            } \
            template<class U> \
            constexpr auto operator op(U&& val) \
                requires requires { get() op std::forward<U>(val); } \
            { \
                return get() op std::forward<U>(val); \
            }

        ALIB6_DEFINE_REF_OP(==)
        ALIB6_DEFINE_REF_OP(<=>)
        ALIB6_DEFINE_REF_OP(!=)
        ALIB6_DEFINE_REF_OP(<)
        ALIB6_DEFINE_REF_OP(<=)
        ALIB6_DEFINE_REF_OP(>)
        ALIB6_DEFINE_REF_OP(>=)

        ALIB6_DEFINE_REF_OP(=)
        ALIB6_DEFINE_REF_OP(+=)
        ALIB6_DEFINE_REF_OP(-=)
        ALIB6_DEFINE_REF_OP(*=)
        ALIB6_DEFINE_REF_OP(/=)
        ALIB6_DEFINE_REF_OP(%=)
        ALIB6_DEFINE_REF_OP(&=)
        ALIB6_DEFINE_REF_OP(|=)
        ALIB6_DEFINE_REF_OP(^=)
        ALIB6_DEFINE_REF_OP(<<=)
        ALIB6_DEFINE_REF_OP(>>=)

        ALIB6_DEFINE_REF_OP(+)
        ALIB6_DEFINE_REF_OP(-)
        ALIB6_DEFINE_REF_OP(*)
        ALIB6_DEFINE_REF_OP(/)
        ALIB6_DEFINE_REF_OP(%)
        ALIB6_DEFINE_REF_OP(&)
        ALIB6_DEFINE_REF_OP(|)
        ALIB6_DEFINE_REF_OP(^)
        ALIB6_DEFINE_REF_OP(<<)
        ALIB6_DEFINE_REF_OP(>>)

        #undef ALIB6_DEFINE_REF_OP

        constexpr auto operator++()    requires requires { ++get(); } { return ++get(); }
        constexpr auto operator++(int) requires requires { get()++; } { return get()++; }
        constexpr auto operator--()    requires requires { --get(); } { return --get(); }
        constexpr auto operator--(int) requires requires { get()--; } { return get()--; }

        template<class Target>
        void write_to_log(Target& target) const {
            if constexpr (requires(const ReferenceType val, Target& tg) { val.write_to_log(tg); }) {
                get().write_to_log(target);
            } else if constexpr (std::formattable<ReferenceType, char>) {
                std::format_to(std::back_inserter(target), "{}", get());
            } else {
                std::format_to(std::back_inserter(target), "RefWrapper(index={}, ptr={})", get_index(), static_cast<const void*>(ptr()));
            }
        }
    };

    /**
     * @brief 构建单层容器索引引用包装器
     */
    template<IndexableContainer Cont>
    [[nodiscard]] constexpr auto ref(Cont& cont, usize index) {
        panic_if(index >= cont.size(), "ref: index out of bounds!");
        return RefWrapper<Cont>{&cont, index};
    }

    /**
     * @brief 多层嵌套容器安全引用包装器 (如 vector<vector<T>>)
     * 操作符完全穿透至底层元素，层级导航与索引设置采用显式方法（如 parent(), child(), next_element()）。
     */
    template<IndexableContainer Cont, usize N>
    struct MultiRefWrapper {
        Cont* cont{nullptr};
        std::array<usize, N> indices{};

    private:
        template<class CurrentCont, usize Depth = 0>
        constexpr decltype(auto) get_raw(CurrentCont& ucont) {
            panic_if(indices[Depth] >= ucont.size(), "MultiRefWrapper: index out of bounds at current depth!");
            if constexpr (Depth == N - 1) {
                return ucont[indices[Depth]];
            } else {
                return get_raw<std::remove_cvref_t<decltype(ucont[indices[Depth]])>, Depth + 1>(ucont[indices[Depth]]);
            }
        }

        template<class CurrentCont, usize Depth = 0>
        constexpr decltype(auto) get_raw(const CurrentCont& ucont) const {
            panic_if(indices[Depth] >= ucont.size(), "MultiRefWrapper: index out of bounds at current depth!");
            if constexpr (Depth == N - 1) {
                return ucont[indices[Depth]];
            } else {
                return get_raw<std::remove_cvref_t<decltype(ucont[indices[Depth]])>, Depth + 1>(ucont[indices[Depth]]);
            }
        }

        template<class CurrentCont, usize Depth = 0, std::integral Arg>
        constexpr void set_indices_raw(CurrentCont& c, Arg&& index) {
            panic_if(static_cast<usize>(index) >= c.size(), "MultiRefWrapper: index out of bounds at current depth!");
            indices[Depth] = static_cast<usize>(index);
        }

        template<class CurrentCont, usize Depth = 0, std::integral Arg, std::integral... Args>
        constexpr void set_indices_raw(CurrentCont& c, Arg&& index, Args&&... iss) {
            panic_if(static_cast<usize>(index) >= c.size(), "MultiRefWrapper: index out of bounds at current depth!");
            indices[Depth] = static_cast<usize>(index);
            set_indices_raw<std::remove_cvref_t<decltype(c[index])>, Depth + 1, Args...>(c[index], std::forward<Args>(iss)...);
        }

    public:
        template<usize N2>
        constexpr MultiRefWrapper(const MultiRefWrapper<Cont, N2>& other)
            requires (N2 >= N)
            : cont(other.cont) {
            std::copy_n(other.indices.begin(), N, indices.begin());
        }

        template<usize dummy>
        constexpr MultiRefWrapper(const MultiRefWrapper<Cont, dummy>& other, usize next_index)
            requires (N == dummy + 1)
            : cont(other.cont) {
            std::copy_n(other.indices.begin(), dummy, indices.begin());
            indices[N - 1] = next_index;
        }

        template<std::integral... Args>
        constexpr MultiRefWrapper(Cont& c, Args&&... args)
            requires (sizeof...(Args) == N)
            : cont(&c) {
            set_indices(std::forward<Args>(args)...);
        }

        constexpr decltype(auto) get() {
            if (!cont) panic("MultiRefWrapper: root container pointer is null!");
            return get_raw<Cont, 0>(*cont);
        }

        constexpr decltype(auto) get() const {
            if (!cont) panic("MultiRefWrapper: root container pointer is null!");
            return get_raw<Cont, 0>(*cont);
        }

        // ==================== 解引用与原生引用穿透 ====================
        constexpr decltype(auto) operator*() { return get(); }
        constexpr decltype(auto) operator*() const { return get(); }

        constexpr auto* operator->() { return std::addressof(get()); }
        constexpr const auto* operator->() const { return std::addressof(get()); }

        constexpr operator decltype(auto)() { return get(); }
        constexpr operator decltype(auto)() const { return get(); }

        // ==================== 下标 [] 与调用 () 透明穿透 ====================
        template<class... Args>
        constexpr decltype(auto) operator[](Args&&... args)
            requires requires { get()[std::forward<Args>(args)...]; }
        {
            return get()[std::forward<Args>(args)...];
        }

        template<class... Args>
        constexpr decltype(auto) operator[](Args&&... args) const
            requires requires { get()[std::forward<Args>(args)...]; }
        {
            return get()[std::forward<Args>(args)...];
        }

        template<class... Args>
        constexpr decltype(auto) operator()(Args&&... args)
            requires requires { get()(std::forward<Args>(args)...); }
        {
            return get()(std::forward<Args>(args)...);
        }

        template<class... Args>
        constexpr decltype(auto) operator()(Args&&... args) const
            requires requires { get()(std::forward<Args>(args)...); }
        {
            return get()(std::forward<Args>(args)...);
        }

        // ==================== 显式层级与索引导航 API ====================
        /// 向上回溯一层父容器引用
        [[nodiscard]] constexpr auto parent() const requires (N > 1) {
            return MultiRefWrapper<Cont, N - 1>(*this);
        }

        /// 向下深入一层子容器引用
        [[nodiscard]] constexpr auto child(usize next_index) const {
            return MultiRefWrapper<Cont, N + 1>(*this, next_index);
        }

        /// 返回替换最内层索引的新引用对象
        [[nodiscard]] constexpr auto with_index(usize new_index) const {
            MultiRefWrapper<Cont, N> ret(*this);
            ret.indices[N - 1] = new_index;
            return ret;
        }

        /// 步进当前最内层索引 (+1)
        constexpr MultiRefWrapper& next_element() {
            return set_index(indices[N - 1] + 1);
        }

        /// 步退当前最内层索引 (-1)
        constexpr MultiRefWrapper& prev_element() {
            return set_index(indices[N - 1] - 1);
        }

        /// 偏移当前最内层索引 (+offset)
        constexpr MultiRefWrapper& advance(isize offset) {
            return set_index(static_cast<usize>(static_cast<isize>(indices[N - 1]) + offset));
        }

        /// 返回偏移当前最内层索引后的新引用对象
        [[nodiscard]] constexpr MultiRefWrapper offset_element(isize offset) const {
            auto t = *this;
            t.set_index(static_cast<usize>(static_cast<isize>(indices[N - 1]) + offset));
            return t;
        }

        /// 设置最内层索引
        constexpr MultiRefWrapper& set_index(usize new_index) {
            indices[N - 1] = new_index;
            get(); // 触发越界断言校验
            return *this;
        }

        template<std::integral... Args>
        constexpr void set_indices(Args&&... args) {
            if (!cont) panic("MultiRefWrapper: root container pointer is null!");
            set_indices_raw<Cont, 0, Args...>(*cont, std::forward<Args>(args)...);
        }

        [[nodiscard]] constexpr usize get_index() const noexcept {
            return indices[N - 1];
        }

        [[nodiscard]] constexpr const std::array<usize, N>& get_indices() const noexcept {
            return indices;
        }

        // ==================== 赋值、算术与比较操作符穿透 ====================
        #define ALIB6_DEFINE_MULTI_REF_OP(op) \
            template<class U> \
            constexpr auto operator op(U&& val) const \
                requires requires { get() op std::forward<U>(val); } \
            { \
                return get() op std::forward<U>(val); \
            } \
            template<class U> \
            constexpr auto operator op(U&& val) \
                requires requires { get() op std::forward<U>(val); } \
            { \
                return get() op std::forward<U>(val); \
            }

        ALIB6_DEFINE_MULTI_REF_OP(==)
        ALIB6_DEFINE_MULTI_REF_OP(<=>)
        ALIB6_DEFINE_MULTI_REF_OP(!=)
        ALIB6_DEFINE_MULTI_REF_OP(<)
        ALIB6_DEFINE_MULTI_REF_OP(<=)
        ALIB6_DEFINE_MULTI_REF_OP(>)
        ALIB6_DEFINE_MULTI_REF_OP(>=)

        ALIB6_DEFINE_MULTI_REF_OP(=)
        ALIB6_DEFINE_MULTI_REF_OP(+=)
        ALIB6_DEFINE_MULTI_REF_OP(-=)
        ALIB6_DEFINE_MULTI_REF_OP(*=)
        ALIB6_DEFINE_MULTI_REF_OP(/=)
        ALIB6_DEFINE_MULTI_REF_OP(%=)
        ALIB6_DEFINE_MULTI_REF_OP(&=)
        ALIB6_DEFINE_MULTI_REF_OP(|=)
        ALIB6_DEFINE_MULTI_REF_OP(^=)
        ALIB6_DEFINE_MULTI_REF_OP(<<=)
        ALIB6_DEFINE_MULTI_REF_OP(>>=)

        ALIB6_DEFINE_MULTI_REF_OP(+)
        ALIB6_DEFINE_MULTI_REF_OP(-)
        ALIB6_DEFINE_MULTI_REF_OP(*)
        ALIB6_DEFINE_MULTI_REF_OP(/)
        ALIB6_DEFINE_MULTI_REF_OP(%)
        ALIB6_DEFINE_MULTI_REF_OP(&)
        ALIB6_DEFINE_MULTI_REF_OP(|)
        ALIB6_DEFINE_MULTI_REF_OP(^)
        ALIB6_DEFINE_MULTI_REF_OP(<<)
        ALIB6_DEFINE_MULTI_REF_OP(>>)

        #undef ALIB6_DEFINE_MULTI_REF_OP

        constexpr auto operator++()    requires requires { ++get(); } { return ++get(); }
        constexpr auto operator++(int) requires requires { get()++; } { return get()++; }
        constexpr auto operator--()    requires requires { --get(); } { return --get(); }
        constexpr auto operator--(int) requires requires { get()--; } { return get()--; }

        template<class Target>
        void write_to_log(Target& target) const {
            using ref_t = decltype(std::declval<const MultiRefWrapper>().get());
            using val_t = std::remove_cvref_t<ref_t>;
            if constexpr (requires(const val_t& val, Target& tg) { val.write_to_log(tg); }) {
                get().write_to_log(target);
            } else if constexpr (std::formattable<val_t, char>) {
                std::format_to(std::back_inserter(target), "{}", get());
            } else {
                std::format_to(std::back_inserter(target), "MultiRefWrapper(ptr={})", static_cast<const void*>(std::addressof(get())));
            }
        }
    };

    /**
     * @brief 构建多层嵌套容器索引引用包装器
     */
    template<IndexableContainer Cont, std::integral... Args>
    [[nodiscard]] constexpr auto refs(Cont& cont, Args&&... indices) {
        return MultiRefWrapper<Cont, sizeof...(Args)>(cont, std::forward<Args>(indices)...);
    }

} // namespace alib6
