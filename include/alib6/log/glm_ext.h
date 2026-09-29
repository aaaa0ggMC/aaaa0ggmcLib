/**
 * @file glm_ext.h
 * @brief GLM 数学类型（vec / qua）的 write_to_log 快速格式化重载（Opt-in）
 * @start-date 2026/09/29
 *
 * @warning 这是普通头文件而非模块分区，必须文本包含；切勿放进任何模块接口单元。
 *
 * 背景：GCC 的模块实现无法在「模块全局片段包含某头文件」与「导入方 TU 再文本
 * 包含同一头文件」之间去重——若 alib6.log:fastfmt 等模块分区在全局片段里
 * #include <glm/gtc/quaternion.hpp>，则任何同时 `import alib6` 且文本使用
 * glm 四元数的 TU 都会因 ext/quaternion_geometric.inl（无包含守卫）触发
 * ODR 重定义错误（glm::dot(qua, qua) / glm::length(qua) 重定义）。
 * 因此 glm 相关格式化从模块接口迁出为本头文件：需要时才 include，
 * 且该 TU 的定义只来自文本路径，不存在模块侧副本，无冲突。
 *
 * @code
 * #include <alib6/log/glm_ext.h>
 * glm::quat q{1, 0, 0, 0};
 * alib6::log::write_to_log(buf, q); // quat(w=1, x=0, y=0, z=0)
 * @endcode
 */
#pragma once
#include <alib6/config.h>

#ifdef ALIB6_HAS_GLM
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace alib6::log {

    template<glm::length_t L, typename T, glm::qualifier Q>
    inline void write_to_log(pmr::string& target, const glm::vec<L, T, Q>& v) {
        target.append("vec");
        target.push_back(static_cast<char>('0' + L));
        target.push_back('(');
        for (glm::length_t i = 0; i < L; ++i) {
            if (i > 0) target.append(", ");
            if constexpr (std::integral<T>) {
                char buf[32];
                auto res = std::to_chars(buf, buf + sizeof(buf), v[i]);
                target.append(buf, static_cast<std::size_t>(res.ptr - buf));
            } else if constexpr (std::floating_point<T>) {
                char buf[64];
                auto res = std::to_chars(buf, buf + sizeof(buf), v[i]);
                target.append(buf, static_cast<std::size_t>(res.ptr - buf));
            } else {
                std::format_to(std::back_inserter(target), "{}", v[i]);
            }
        }
        target.push_back(')');
    }

    template<typename T, glm::qualifier Q>
    inline void write_to_log(pmr::string& target, const glm::qua<T, Q>& q) {
        target.append("quat(w=");
        char buf[64];
        auto res = std::to_chars(buf, buf + sizeof(buf), q.w);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(", x=");
        res = std::to_chars(buf, buf + sizeof(buf), q.x);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(", y=");
        res = std::to_chars(buf, buf + sizeof(buf), q.y);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.append(", z=");
        res = std::to_chars(buf, buf + sizeof(buf), q.z);
        target.append(buf, static_cast<std::size_t>(res.ptr - buf));
        target.push_back(')');
    }

} // namespace alib6::log

#endif // ALIB6_HAS_GLM
