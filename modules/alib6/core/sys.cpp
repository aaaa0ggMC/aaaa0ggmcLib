/**
 * @file sys.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 系统信息与硬件监控实现 (隐藏底层操作系统 API)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6::sys {

#ifdef _WIN32
    void enable_virtual_terminal() noexcept {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode;
        if (GetConsoleMode(hConsole, &consoleMode)) {
            consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hConsole, consoleMode);
        }
    }

    namespace {
        struct AutoFix {
            AutoFix() noexcept {
                enable_virtual_terminal();
            }

            ~AutoFix() {
                // 退出时静默重置颜色，防止污染用户的终端环境
                std::fputs("\033[0m", stdout);
            }
        };

        static AutoFix __autofix_trigger;
    }
#else
    void enable_virtual_terminal() noexcept {}
#endif

    pmr::string get_cpu_brand(memory_resource* mem) {
#ifdef _WIN32
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Hardware\\Description\\System\\CentralProcessor\\0", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            char tchData[1024] = {0};
            DWORD dwSize = sizeof(tchData);
            if (RegQueryValueExA(hKey, "ProcessorNameString", 0, nullptr, reinterpret_cast<LPBYTE>(tchData), &dwSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return pmr::string(tchData, mem);
            }
            RegCloseKey(hKey);
        }
        return pmr::string("Unknown", mem);
#else
        if (FILE* f = std::fopen("/proc/cpuinfo", "r")) {
            char line[256];
            pmr::string brand("Unknown", mem);
            while (std::fgets(line, sizeof(line), f)) {
                std::string_view sv(line);
                if (sv.starts_with("model name")) {
                    auto pos = sv.find(':');
                    if (pos != std::string_view::npos) {
                        sv = sv.substr(pos + 1);
                        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) sv.remove_prefix(1);
                        while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r' || sv.back() == ' ')) sv.remove_suffix(1);
                        brand = pmr::string(sv, mem);
                        break;
                    }
                }
            }
            std::fclose(f);
            return brand;
        }
        return pmr::string("Unknown", mem);
#endif
    }

    ProgramMemUsage get_prog_mem_usage() noexcept {
        ProgramMemUsage usage{0, 0};
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            usage.memory = static_cast<mem_bytes>(pmc.WorkingSetSize);
            usage.virt_memory = static_cast<mem_bytes>(pmc.PagefileUsage);
        }
#else
        if (FILE* f = std::fopen("/proc/self/status", "r")) {
            char line[128];
            int found = 0;
            while (std::fgets(line, sizeof(line), f) && found < 2) {
                long val = 0;
                if (std::sscanf(line, "VmSize: %ld", &val) == 1) {
                    usage.virt_memory = static_cast<mem_bytes>(val) * 1024;
                    ++found;
                } else if (std::sscanf(line, "VmRSS: %ld", &val) == 1) {
                    usage.memory = static_cast<mem_bytes>(val) * 1024;
                    ++found;
                }
            }
            std::fclose(f);
        }
#endif
        return usage;
    }

    GlobalMemUsage get_global_mem_usage() noexcept {
        GlobalMemUsage ret{};
#ifdef _WIN32
        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);
        if (GlobalMemoryStatusEx(&statex)) {
            ret.physical_total = statex.ullTotalPhys;
            ret.physical_used  = statex.ullTotalPhys - statex.ullAvailPhys;
            ret.virtual_total  = statex.ullTotalVirtual;
            ret.virtual_used   = statex.ullTotalVirtual - statex.ullAvailVirtual;
            ret.page_total     = statex.ullTotalPageFile;
            ret.page_used      = statex.ullTotalPageFile - statex.ullAvailPageFile;
            ret.percent        = statex.dwMemoryLoad;
        }
#else
        if (FILE* f = std::fopen("/proc/meminfo", "r")) {
            char line[128];
            long long total = 0, free = 0, buffers = 0, cached = 0, reclaimable = 0, s_total = 0, s_free = 0;
            while (std::fgets(line, sizeof(line), f)) {
                long long val = 0;
                if (std::sscanf(line, "MemTotal: %lld", &val) == 1) total = val;
                else if (std::sscanf(line, "MemFree: %lld", &val) == 1) free = val;
                else if (std::sscanf(line, "Buffers: %lld", &val) == 1) buffers = val;
                else if (std::sscanf(line, "Cached: %lld", &cached) == 1) cached = val;
                else if (std::sscanf(line, "SReclaimable: %lld", &reclaimable) == 1) reclaimable = val;
                else if (std::sscanf(line, "SwapTotal: %lld", &s_total) == 1) s_total = val;
                else if (std::sscanf(line, "SwapFree: %lld", &s_free) == 1) s_free = val;
            }
            std::fclose(f);

            long long available = free + buffers + cached + reclaimable;
            ret.physical_total = total * 1024;
            ret.physical_used  = (total > available ? (total - available) : 0) * 1024;
            ret.page_total     = s_total * 1024;
            ret.page_used      = (s_total > s_free ? (s_total - s_free) : 0) * 1024;
            ret.virtual_total  = 0;
            ret.virtual_used   = 0;

            if (ret.physical_total > 0) {
                ret.percent = static_cast<u32>((ret.physical_used * 100) / ret.physical_total);
            }
        }
#endif
        return ret;
    }

} // namespace alib6::sys
