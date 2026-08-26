/**
 * @file log.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 日志系统重型实现 (遵循 alib5 消息主遍历分发、static thread_local 复用、ANSI 颜色与后台消费者线程)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <climits>
#include <cstdio>
#include <ctime>
#include <stacktrace>

module alib6.log;
import std;
import alib6.core;

namespace pmr = std::pmr;

namespace alib6::log {

    // ==================== LogMsg 线程局部缓冲实现 ====================

    std::string& LogMsg::sdate() {
        static thread_local std::string sdate_buf;
        return sdate_buf;
    }

    std::string& LogMsg::scomposed() {
        static thread_local std::string scomposed_buf;
        return scomposed_buf;
    }

    void LogMsg::build_on_consumer() {
        if (cfg.disable_extra_information) return;
        [[maybe_unused]] thread_local static bool inited = [&] {
            sdate().resize(date_str_resize);
            return false;
        }();

        if (cfg.gen_date) {
            static thread_local std::time_t old_time = 0;
            std::time_t rawtime = std::time(nullptr);
            if (rawtime != old_time) {
                struct tm ptminfo{};
                #ifdef _WIN32
                localtime_s(&ptminfo, &rawtime);
                #else
                localtime_r(&rawtime, &ptminfo);
                #endif
                sdate().clear();
                sdate().resize(date_str_resize);
                int len = snprintf(
                    sdate().data(), date_str_resize, "%04d-%02d-%02d %02d:%02d:%02d",
                    ptminfo.tm_year + 1900, ptminfo.tm_mon + 1, ptminfo.tm_mday,
                    ptminfo.tm_hour, ptminfo.tm_min, ptminfo.tm_sec
                );
                if (len > 0) {
                    sdate().resize(static_cast<usize>(len));
                }
                old_time = rawtime;
            }
        }
    }

    std::string_view LogMsg::gen_composed() {
        if (cfg.disable_extra_information) {
            body.push_back('\n');
            return body;
        }
        [[maybe_unused]] static thread_local bool inited = [&] {
            scomposed().clear();
            scomposed().resize(compose_str_resize);
            return false;
        }();

        if (generated) return scomposed();

        scomposed().clear();
        if (scomposed().capacity() < compose_str_resize) {
            scomposed().reserve(compose_str_resize);
        }

        if (cfg.gen_date) {
            std::format_to(std::back_inserter(scomposed()), "[{}]", sdate());
        }
        if (cfg.out_level && cfg.level_cast) {
            std::format_to(std::back_inserter(scomposed()), "[{}]", cfg.level_cast(level));
        }
        if (cfg.out_header && !header.empty()) {
            std::format_to(std::back_inserter(scomposed()), "[{}]", header);
        }
        if (cfg.gen_time) {
            std::format_to(std::back_inserter(scomposed()), "[{:.2f}ms]", timestamp);
        }
        if (cfg.gen_thread_id) {
            std::format_to(std::back_inserter(scomposed()), "[TID{:x}]", thread_id);
        }
        scomposed().append(": ");
        scomposed().append(body);
        scomposed().append(cfg.separator);

        generated = true;
        return scomposed();
    }

    // ==================== log_bin 实现 ====================

    void log_bin::write_to_log(pmr::string& target) const {
        if (!data || size == 0) return;

        constexpr std::string_view s_bin = "01";
        constexpr std::string_view s_oct = "01234567";
        constexpr std::string_view s_hex_lower = "0123456789abcdef";
        constexpr std::string_view s_hex_upper = "0123456789ABCDEF";
        std::string_view s_hex = config.capital ? s_hex_upper : s_hex_lower;

        const auto* cursor = static_cast<const unsigned char*>(data);
        usize cycle_len = 0;

        auto check_split = [&]() {
            if (config.split_when > 0 && ++cycle_len >= config.split_when) {
                cycle_len = 0;
                target.append(config.split_str);
            }
        };

        for (usize i = 0; i < size; ++i) {
            unsigned char ch = cursor[i];

            switch (config.fmt) {
                case Hex: {
                    target.push_back(s_hex[(ch >> 4) & 0x0F]);
                    check_split();
                    target.push_back(s_hex[ch & 0x0F]);
                    check_split();
                    break;
                }
                case Oct: {
                    target.push_back(s_oct[(ch >> 6) & 0x03]);
                    check_split();
                    target.push_back(s_oct[(ch >> 3) & 0x07]);
                    check_split();
                    target.push_back(s_oct[ch & 0x07]);
                    check_split();
                    break;
                }
                case Bin: {
                    for (int b = CHAR_BIT - 1; b >= 0; --b) {
                        target.push_back(s_bin[(ch >> b) & 1]);
                        check_split();
                    }
                    break;
                }
            }
        }
    }

    // ==================== log_rate 实现 ====================

    bool log_rate::Info::check() noexcept {
        switch (type) {
            case Rate: {
                if (times_per_second > 0.0) {
                    auto now = std::chrono::steady_clock::now();
                    double elapsed_ms = static_cast<double>(
                        std::chrono::duration_cast<std::chrono::microseconds>(now - start_tp).count()
                    ) / 1000.0;
                    double interval_ms = 1000.0 / times_per_second;
                    double last = last_time.load(std::memory_order::relaxed);

                    if (elapsed_ms - last >= interval_ms) {
                        if (last_time.compare_exchange_strong(last, elapsed_ms, std::memory_order::relaxed)) {
                            return true;
                        }
                    }
                    return false;
                }
                return true;
            }
            case Once: {
                if (printed.load(std::memory_order::relaxed)) return false;
                bool expected = false;
                return printed.compare_exchange_strong(expected, true, std::memory_order::release);
            }
        }
        return true;
    }

    // ==================== log_stacktrace 实现 ====================

    void log_stacktrace::write_to_log(pmr::string& str) const {
        auto trace = std::stacktrace::current();
        usize idx = 0;
        for (const auto& frame : trace) {
            if (idx++ < skip_depth) continue;
            if (frame.source_file().empty() && frame.source_line() == 0) continue;

            std::format_to(
                std::back_inserter(str),
                "  #{} {}:{} at {}{}",
                idx - skip_depth - 1,
                frame.source_file(),
                frame.source_line(),
                frame.description(),
                auto_newline ? "\n" : ""
            );
        }
    }

    // ==================== Console 彩色渲染实现 ====================

    static void render_color_console(std::string& s_buffer, u16 category_id, ConsoleConfig& cfg, LogMsg& msg) {
        auto append_ansi_payload = [](std::string& buf, u64 payload) {
            if (payload == 0) {
                buf.append("\033[0m");
                return;
            }

            buf.append("\033[");
            bool first = true;

            // 样式 Style 位掩码 (Bits 16-31)
            u16 style = static_cast<u16>((payload >> 16) & 0xFFFF);
            for (int i = 0; i < 8; ++i) {
                if (style & (1 << i)) {
                    if (!first) buf.push_back(';');
                    buf.push_back(static_cast<char>('1' + i));
                    first = false;
                }
            }

            // 背景色 (Bits 8-15)
            u8 bg = static_cast<u8>((payload >> 8) & 0xFF);
            if (bg) {
                if (!first) buf.push_back(';');
                std::format_to(std::back_inserter(buf), "{}", bg);
                first = false;
            }

            // 前景色 (Bits 0-7)
            u8 fg = static_cast<u8>(payload & 0xFF);
            if (fg) {
                if (!first) buf.push_back(';');
                std::format_to(std::back_inserter(buf), "{}", fg);
            }
            buf.push_back('m');
        };

        if (!msg.cfg.disable_extra_information) {
            if (msg.cfg.gen_date) {
                s_buffer.append("[");
                if (cfg.date_color_schema) s_buffer.append(cfg.date_color_schema(msg));
                s_buffer.append(msg.sdate());
                if (cfg.date_color_schema) s_buffer.append("\033[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.out_level && msg.cfg.level_cast) {
                s_buffer.append("[");
                if (cfg.level_color_schema) s_buffer.append(cfg.level_color_schema(msg));
                s_buffer.append(msg.cfg.level_cast(msg.level));
                if (cfg.level_color_schema) s_buffer.append("\033[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.out_header && !msg.header.empty()) {
                s_buffer.append("[");
                if (cfg.head_color_schema) s_buffer.append(cfg.head_color_schema(msg));
                s_buffer.append(msg.header);
                if (cfg.head_color_schema) s_buffer.append("\033[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.gen_time) {
                s_buffer.append("[");
                if (cfg.time_color_schema) s_buffer.append(cfg.time_color_schema(msg));
                std::format_to(std::back_inserter(s_buffer), "{:.2f}ms", msg.timestamp);
                if (cfg.time_color_schema) s_buffer.append("\033[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.gen_thread_id) {
                s_buffer.append("[");
                if (cfg.thread_id_color_schema) s_buffer.append(cfg.thread_id_color_schema(msg));
                std::format_to(std::back_inserter(s_buffer), "TID{:x}", msg.thread_id);
                if (cfg.thread_id_color_schema) s_buffer.append("\033[0m");
                s_buffer.append("]");
            }

            s_buffer.append(": ");
        }

        usize last_pos = 0;
        std::string_view body_view = msg.body;
        u64 active_payload = 0;

        if (cfg.body_color_schema) s_buffer.append(cfg.body_color_schema(msg));
        for (const auto& tag : msg.tags) {
            if (tag.category == category_id && tag.valid()) {
                usize current_pos = static_cast<usize>(tag.get_pos());
                if (current_pos > last_pos && current_pos <= body_view.size()) {
                    s_buffer.append(body_view.substr(last_pos, current_pos - last_pos));
                    last_pos = current_pos;
                }
                if (tag.payload != active_payload) {
                    append_ansi_payload(s_buffer, tag.payload);
                    active_payload = tag.payload;
                }
            }
        }

        if (last_pos < body_view.size()) {
            s_buffer.append(body_view.substr(last_pos));
        }

        if (active_payload != 0 || cfg.body_color_schema) {
            s_buffer.append("\033[0m");
        }
        s_buffer.append(msg.cfg.separator);
    }

    void Console::write(LogMsg& msg) {
        if (!out) return;

        if (support_colors) {
            static thread_local std::string s_buffer;
            s_buffer.clear();
            if (s_buffer.capacity() < compose_str_resize) s_buffer.reserve(compose_str_resize);
            render_color_console(s_buffer, category_id, cfg, msg);

            std::lock_guard<std::mutex> lock(console_lock);
            std::fwrite(s_buffer.data(), 1, s_buffer.size(), out);
        } else {
            auto data = msg.gen_composed();
            std::lock_guard<std::mutex> lock(console_lock);
            std::fwrite(data.data(), 1, data.size(), out);
        }
    }

    template<class StrT>
    void ConsoleBuffer<StrT>::write(LogMsg& msg) {
        if (!data) return;
        static thread_local std::string s_buffer;
        s_buffer.clear();
        if (s_buffer.capacity() < compose_str_resize) s_buffer.reserve(compose_str_resize);
        render_color_console(s_buffer, category_id, cfg, msg);

        std::lock_guard<std::mutex> lk(buffer_lock);
        if (data) {
            data->set_value(StrT(s_buffer));
            data = nullptr;
        }
    }

    template<class StrT>
    void SyncConsoleBuffer<StrT>::write(LogMsg& msg) {
        static thread_local std::string s_buffer;
        s_buffer.clear();
        if (s_buffer.capacity() < compose_str_resize) s_buffer.reserve(compose_str_resize);
        render_color_console(s_buffer, category_id, cfg, msg);

        std::lock_guard<std::mutex> lk(buffer_lock);
        data = StrT(s_buffer);
        ++version_code;
        cv.notify_all();
    }

    template struct ConsoleBuffer<std::string>;
    template struct SyncConsoleBuffer<std::string>;

    // ==================== Logger 核心实现 ====================

    Logger::Logger(const LoggerConfig& cfg, memory_resource* mem)
        : config(cfg)
        , mem_res(mem)
        , targets(mem)
        , search_targets(mem)
        , filters(mem)
        , search_filters(mem)
        , header_pool(mem)
        , messages(mem)
        , logger_running(true)
        , start_time(std::chrono::steady_clock::now()) {
        back_pressure_threshold = static_cast<usize>(config.back_pressure_multiply) *
                                  config.fetch_message_count_max *
                                  std::max(1u, config.consumer_count);
        setup_consumer_threads();
    }

    Logger::~Logger() {
        logger_running = false;
        cv.notify_all();
        consumers.clear(); // 等待所有 jthread 退出
        flush();
    }

    void Logger::setup_consumer_threads() {
        for (u32 i = 0; i < config.consumer_count; ++i) {
            consumers.emplace_back(&Logger::consumer_func, this);
        }
    }

    void Logger::consumer_func() {
        std::vector<LogMsg> target_batch;
        target_batch.reserve(config.fetch_message_count_max);

        while (true) {
            {
                std::unique_lock<std::mutex> lock(msg_lock);
                cv.wait(lock, [this] {
                    return !messages.empty() || !logger_running;
                });

                if (!logger_running && messages.empty()) {
                    return;
                }

                usize fetch_size = std::min(
                    static_cast<usize>(config.fetch_message_count_max),
                    messages.size()
                );
                target_batch.clear();
                for (usize i = 0; i < fetch_size; ++i) {
                    target_batch.push_back(std::move(messages.front()));
                    messages.pop_front();
                }
                message_size.fetch_sub(fetch_size, std::memory_order::relaxed);
                active_consumers.fetch_add(1, std::memory_order::relaxed);

                if (config.consumer_count > 1 && !messages.empty()) {
                    cv.notify_one();
                }
            }

            write_messages(std::span(target_batch), false);
            active_consumers.fetch_sub(1, std::memory_order::relaxed);
            cv_flush.notify_all();
        }
    }

    usize Logger::fetch_messages(std::vector<LogMsg>& target) {
        std::lock_guard<std::mutex> lock(msg_lock);
        if (messages.empty()) return 0;

        usize fetch_size = std::min(
            static_cast<usize>(config.fetch_message_count_max),
            messages.size()
        );
        target.clear();
        target.reserve(fetch_size);

        for (usize i = 0; i < fetch_size; ++i) {
            target.push_back(std::move(messages.front()));
            messages.pop_front();
        }
        message_size.fetch_sub(fetch_size, std::memory_order::relaxed);
        return fetch_size;
    }

    void Logger::flush_targets() {
        std::lock_guard<std::mutex> lock(mod_lock);
        for (auto& target : targets) {
            if (target && target->enabled) {
                target->flush();
            }
        }
    }

    void Logger::flush() {
        if (config.consumer_count > 0) {
            std::unique_lock<std::mutex> lock(msg_lock);
            cv.notify_all();
            cv_flush.wait(lock, [this] {
                return messages.empty() && active_consumers.load(std::memory_order::relaxed) == 0;
            });
        } else {
            std::vector<LogMsg> msgs;
            while (true) {
                usize count = fetch_messages(msgs);
                if (count == 0) break;
                write_messages(std::span(msgs).subspan(0, count), false);
            }
        }
        flush_targets();
    }

    void Logger::write_messages(std::span<LogMsg> msgs, bool autoflush) {
        if (!filters.empty()) {
            for (auto& msg : msgs) {
                if (!msg.m_nice_one) continue;
                for (auto& filter : filters) {
                    if (!filter || !filter->enabled) continue;
                    msg.m_nice_one = filter->filter(msg);
                    if (!msg.m_nice_one) break;
                }
            }
        }

        // 关键：按 message 作为外层主遍历，先执行 build_on_consumer，所有 target 共享由 static thread_local 生成的 composed
        for (auto& msg : msgs) {
            if (!msg.m_nice_one) continue;
            msg.build_on_consumer();

            for (auto& target : targets) {
                if (target && target->enabled) {
                    target->write(msg);
                }
            }
        }

        if (autoflush) {
            flush_targets();
        }
    }

    bool Logger::push_message_pmr(
        int level,
        std::string_view head,
        pmr::string&& body,
        const LogMsgConfig& cfg,
        pmr::vector<LogCustomTag>* tags
    ) {
        for (auto& filter : filters) {
            if (filter && filter->enabled) {
                if (!filter->pre_filter(level, body, cfg)) return false;
            }
        }

        if (config.consumer_count > 0) {
            usize cur_size = 0;
            {
                std::lock_guard<std::mutex> lock(msg_lock);
                auto& msg = messages.emplace_back(head, level, std::move(body), cfg, tags);
                msg.build_on_producer(start_time);
                cur_size = message_size.fetch_add(1, std::memory_order::relaxed) + 1;

                if (cur_size > config.maximum_message_count) [[unlikely]] {
                    usize drop_count = config.maximum_message_count / 2;
                    messages.erase(messages.begin(), messages.begin() + drop_count);
                    message_size.store(messages.size(), std::memory_order::relaxed);
                    cur_size = messages.size();
                }
            }
            cv.notify_one();

            if (config.enable_back_pressure && cur_size >= back_pressure_threshold) {
                static thread_local std::vector<LogMsg> digest_batch;
                usize count = fetch_messages(digest_batch);
                if (count > 0) {
                    write_messages(std::span(digest_batch).subspan(0, count), false);
                    digest_batch.clear();
                }
            }
        } else {
            // 同步模式
            LogMsg msg(head, level, std::move(body), cfg, tags);
            msg.build_on_producer(start_time);
            write_messages(std::span(&msg, 1), false);
        }
        return true;
    }

    bool Logger::push_message(int level, std::string_view header, std::string_view body, const LogMsgConfig& cfg) {
        pmr::string str(body, mem_res);
        return push_message_pmr(level, header, std::move(str), cfg);
    }

    std::string_view Logger::register_header(std::string_view val) {
        if (val.empty()) return "";
        std::lock_guard<std::mutex> lock(header_pool_lock);
        pmr::string s(val, mem_res);
        auto it = header_pool.find(s);
        if (it != header_pool.end()) {
            return *it;
        }
        return *header_pool.emplace(std::move(s)).first;
    }

} // namespace alib6::log
