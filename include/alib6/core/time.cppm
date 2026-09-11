/**
 * @file time.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 时间格式化与耗时度量工具 (接口定义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:time;
import std;
import :types;
import :memory;

namespace pmr = std::pmr;

export namespace alib6::time {

    /**
     * @brief 获取当前时间的格式化字符串 (YYYY-MM-DD HH:MM:SS)
     */
    [[nodiscard]] pmr::string get_time(memory_resource* mem = get_default_resource());

    /**
     * @brief 对耗时进行自适应单位归一化换算 (s, ms, us, ns)
     */
    [[nodiscard]] std::pair<double, std::string_view> normalize_elapse(double t_ms) noexcept;

    /**
     * @brief 格式化秒数为可读持续时间 (如: 1y 2d 3h 4m 5s)
     */
    [[nodiscard]] pmr::string format_duration(i64 total_secs, memory_resource* mem = get_default_resource());

} // namespace alib6::time

export namespace alib6::misc {
    using alib6::time::normalize_elapse;
}

export namespace alib6 {
    namespace misc = alib6::misc;
}
