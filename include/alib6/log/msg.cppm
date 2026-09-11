/**
 * @file msg.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 日志消息载体结构体 (按消息分发、static thread_local 组合缓冲、PMR 内存与 100% Move 语义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <ctime>

export module alib6.log:msg;
import std;
import alib6.core;
import :config;

namespace pmr = std::pmr;

export namespace alib6::log {

    class Logger;

    constexpr unsigned int date_str_resize = 32;
    constexpr unsigned int compose_str_resize = 1024;

    /**
     * @brief 日志消息实体
     * 遵循 alib5 架构：在消费者端按消息循环遍历（而非按 targets 遍历），
     * 配合 static thread_local 缓冲一次性生成 composed 文本供所有 target 共享读取。
     */
    struct LogMsg {
        friend class Logger;

    private:
        /// @brief 标识日志是否通过过滤器 (true = 正常消费, false = 丢弃)
        bool m_nice_one{true};
        /// @brief 标识当前日志是否已经生成了组合字符串以避免重复构建
        bool generated{false};

    public:
        /// @brief 标识所属模块头 (指向 Logger 内置的常量池，生命周期由 Logger 保障绝不悬垂)
        std::string_view header{""};
        /// @brief 日志主体文本 (由 PMR 内存池分配驱动)
        pmr::string body;
        /// @brief 当前消息绑定的格式配置
        LogMsgConfig cfg{};
        /// @brief 日志严重级别
        int level{0};

        /// @brief 生产者线程 ID
        u64 thread_id{0};
        /// @brief 相对启动时刻的耗时 (毫秒)
        double timestamp{0.0};
        /// @brief 用户附加自定义标签列表 (PMR 隔离)
        pmr::vector<LogCustomTag> tags;

        //// 线程局部缓冲 (复用同一线程内的临时构建空间) ////
        static std::string& sdate();
        static std::string& scomposed();

        LogMsg() = default;

        explicit LogMsg(
            const pmr::polymorphic_allocator<char>& str_alloc,
            const pmr::polymorphic_allocator<LogCustomTag>& tag_alloc,
            const LogMsgConfig& c = LogMsgConfig{}
        ) : m_nice_one(true)
          , generated(false)
          , body(str_alloc)
          , cfg(c)
          , tags(tag_alloc) {}

        LogMsg(
            std::string_view head,
            int lvl,
            pmr::string&& b,
            const LogMsgConfig& c,
            pmr::vector<LogCustomTag>* t = nullptr
        ) : m_nice_one(true)
          , generated(false)
          , header(head)
          , body(std::move(b))
          , cfg(c)
          , level(lvl)
          , tags(t && !t->empty() ? std::move(*t) : pmr::vector<LogCustomTag>(body.get_allocator().resource())) {}

        // 禁止隐式深拷贝，强制沿队列全程进行 std::move 零拷贝转移
        LogMsg(const LogMsg&) = delete;
        LogMsg& operator=(const LogMsg&) = delete;

        LogMsg(LogMsg&& other) noexcept
            : m_nice_one(other.m_nice_one)
            , generated(other.generated)
            , header(other.header)
            , body(std::move(other.body))
            , cfg(other.cfg)
            , level(other.level)
            , thread_id(other.thread_id)
            , timestamp(other.timestamp)
            , tags(std::move(other.tags)) {}

        LogMsg& operator=(LogMsg&& other) noexcept {
            if (this != &other) {
                m_nice_one = other.m_nice_one;
                generated = other.generated;
                header = other.header;
                body = std::move(other.body);
                cfg = other.cfg;
                level = other.level;
                thread_id = other.thread_id;
                timestamp = other.timestamp;
                tags = std::move(other.tags);
            }
            return *this;
        }

        /**
         * @brief 生产者阶段填充耗时与线程 ID
         */
        void build_on_producer(std::chrono::steady_clock::time_point start_tp) noexcept {
            if (cfg.disable_extra_information) return;

            if (cfg.gen_thread_id) {
                thread_local static const u64 cached_tid = static_cast<u64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                thread_id = cached_tid;
            }

            if (cfg.gen_time) {
                auto now = std::chrono::steady_clock::now();
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - start_tp).count();
                timestamp = static_cast<double>(us) / 1000.0;
            }
        }

        /**
         * @brief 消费者阶段构建日期字符串
         */
        void build_on_consumer();

        /**
         * @brief 生成组合文本 (懒加载模式，同一消息首个 target 触发构建后直接复用)
         */
        std::string_view gen_composed();

        /**
         * @brief 清除生成标记 (供重置/复用)
         */
        void clear_gen_flag() noexcept {
            generated = false;
        }

        [[nodiscard]] bool is_valid() const noexcept {
            return m_nice_one;
        }

        void set_valid(bool val) noexcept {
            m_nice_one = val;
        }

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "LogMsg(level={}, body=\"{}\")", level, body);
        }
    };

} // namespace alib6::log
