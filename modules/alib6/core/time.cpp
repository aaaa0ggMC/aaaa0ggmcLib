/**
 * @file time.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 时间格式化与耗时度量实现
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <ctime>

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6::time {

    constexpr std::array<std::string_view, 4> normalize_elapse_movers = {"s", "ms", "us", "ns"};
    constexpr usize normalize_elapse_ms_index = 1;

    pmr::string get_time(memory_resource* mem) {
        pmr::string buffer(mem);
        std::time_t rawtime;
        struct std::tm ptminfo{};
        std::time(&rawtime);

#ifdef _WIN32
        localtime_s(&ptminfo, &rawtime);
#else
        localtime_r(&rawtime, &ptminfo);
#endif

        std::format_to(
            std::back_inserter(buffer),
            "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
            ptminfo.tm_year + 1900,
            ptminfo.tm_mon + 1,
            ptminfo.tm_mday,
            ptminfo.tm_hour,
            ptminfo.tm_min,
            ptminfo.tm_sec
        );
        return buffer;
    }

    std::pair<double, std::string_view> normalize_elapse(double t_ms) noexcept {
        usize mindex = normalize_elapse_ms_index;
        while (mindex > 0 && mindex < (normalize_elapse_movers.size() - 1)) {
            if (t_ms < 1.0) {
                mindex += 1;
                t_ms *= 1000.0;
            } else if (t_ms >= 1000.0) {
                mindex -= 1;
                t_ms /= 1000.0;
            } else {
                break;
            }
        }
        return {t_ms, normalize_elapse_movers[mindex]};
    }

    pmr::string format_duration(i64 total_secs, memory_resource* mem) {
        pmr::string buffer(mem);
        if (total_secs <= 0) {
            buffer = "0s";
            return buffer;
        }

        i64 sec  = total_secs % 60; total_secs /= 60;
        i64 min  = total_secs % 60; total_secs /= 60;
        i64 hour = total_secs % 24; total_secs /= 24;
        i64 day  = total_secs % 365;
        i64 year = total_secs / 365;

        if (year > 0) std::format_to(std::back_inserter(buffer), "{}y ", year);
        if (day > 0)  std::format_to(std::back_inserter(buffer), "{}d ", day);
        if (hour > 0) std::format_to(std::back_inserter(buffer), "{}h ", hour);
        if (min > 0)  std::format_to(std::back_inserter(buffer), "{}m ", min);
        if (sec > 0 || buffer.empty()) std::format_to(std::back_inserter(buffer), "{}s", sec);

        if (!buffer.empty() && buffer.back() == ' ') {
            buffer.pop_back();
        }
        return buffer;
    }

} // namespace alib6::time
