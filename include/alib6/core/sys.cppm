/**
 * @file sys.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 系统信息获取、硬件监控与终端配置 (接口定义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.core:sys;
import std;
import :types;
import :memory;

namespace pmr = std::pmr;

export namespace alib6::sys {

    /**
     * @brief 启用 Windows 虚拟终端 VT100 颜色转义序列支持 (非 Windows 为 no-op)
     */
    void enable_virtual_terminal() noexcept;

    /**
     * @brief 进程内存占用统计结构体 (单位: 字节)
     */
    struct ProgramMemUsage {
        mem_bytes memory{0};      ///< 物理内存占用 (RSS / Working Set)
        mem_bytes virt_memory{0}; ///< 虚拟内存占用 (VmSize / Pagefile)
    };

    /**
     * @brief 系统全局内存统计结构体 (单位: 字节)
     */
    struct GlobalMemUsage {
        u32 percent{0};             ///< 占用百分比
        mem_bytes physical_total{0};///< 物理内存总量
        mem_bytes physical_used{0}; ///< 物理内存已用量
        mem_bytes virtual_total{0}; ///< 虚拟内存总量
        mem_bytes virtual_used{0};  ///< 虚拟内存已用量
        mem_bytes page_total{0};    ///< 交换区/分页文件总量
        mem_bytes page_used{0};     ///< 交换区/分页文件已用量
    };

    /**
     * @brief 获取当前 CPU 品牌/型号字符串
     */
    [[nodiscard]] pmr::string get_cpu_brand(memory_resource* mem = get_default_resource());

    /**
     * @brief 获取当前进程内存占用
     */
    [[nodiscard]] ProgramMemUsage get_prog_mem_usage() noexcept;

    /**
     * @brief 获取系统全局内存占用情况
     */
    [[nodiscard]] GlobalMemUsage get_global_mem_usage() noexcept;

} // namespace alib6::sys
