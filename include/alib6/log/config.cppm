/**
 * @file config.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 日志系统配置结构体、严重性级别与自定义标签定义
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.log:config;
import std;
import alib6.core;

namespace pmr = std::pmr;

export namespace alib6::log {

    /// @brief 日志严重级别枚举
    enum class LogLevel : i32 {
        Trace = 0,
        Debug = 1,
        Info  = 2,
        Warn  = 3,
        Error = 4,
        Fatal = 5
    };

    /// @brief 严重级别别名兼容
    using Severity = LogLevel;

    /**
     * @brief 获取日志级别对应的标准文本标识
     */
    [[nodiscard]] constexpr std::string_view get_log_level_name(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            default:              return "UNKNOWN";
        }
    }

    [[nodiscard]] constexpr std::string_view get_log_level_name(int level) noexcept {
        return get_log_level_name(static_cast<LogLevel>(level));
    }

    /// @brief 每条日志允许的最大自定义标签数量
    constexpr u32 log_custom_tag_count = 8;

    /**
     * @brief 用户自定义标签 (挂载在日志消息指定字符位置的元数据)
     */
    struct LogCustomTag {
        /// @brief 插入位置偏移 (基于 1 的偏移量，0 表示无效)
        u64 pos{0};
        /// @brief 16 位类别标识
        u64 category : 16 {0};
        /// @brief 48 位载荷数据
        u64 payload  : 48 {0};

        constexpr LogCustomTag() noexcept = default;
        constexpr LogCustomTag(u16 cat, u64 pay, u64 p = 0) noexcept
            : pos(p == 0 ? 0 : p + 1), category(cat), payload(pay) {}

        /**
         * @brief 设置基于 0 的字符位置
         */
        constexpr void set_pos(u64 p) noexcept { pos = p + 1; }
        constexpr void set(u64 p) noexcept { pos = p + 1; }

        /**
         * @brief 获取基于 0 的字符位置
         */
        [[nodiscard]] constexpr u64 get_pos() const noexcept { return pos > 0 ? pos - 1 : 0; }
        [[nodiscard]] constexpr u64 get() const noexcept { return pos > 0 ? pos - 1 : 0; }

        /**
         * @brief 判定当前标签是否包含有效位置
         */
        [[nodiscard]] constexpr bool valid() const noexcept { return pos != 0; }

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "LogCustomTag(pos: {}, cat: {}, payload: {})", get_pos(), category, payload);
        }
    };

    /**
     * @brief 单条日志消息的输出格式配置
     */
    struct LogMsgConfig {
        using LevelCastFn = std::string_view (*)(int);

        /// @brief 是否完全禁用额外元信息（日期、时间、线程ID、Header等）的格式化
        bool disable_extra_information{false};

        /// @brief 是否生成线程 ID (例如 [TID12345])
        bool gen_thread_id{false};
        /// @brief 是否生成相对启动时间的运行耗时 (例如 [12.34ms])
        bool gen_time{true};
        /// @brief 是否生成当前日期时间 (例如 [2026-08-26 12:00:00])
        bool gen_date{true};

        /// @brief 是否输出 LogFactory 标识头 (例如 [AuthServer])
        bool out_header{true};
        /// @brief 是否输出日志严重性级别 (例如 [INFO])
        bool out_level{true};
        /// @brief 自定义级别转文本函数指针
        LevelCastFn level_cast{get_log_level_name};

        /// @brief 每条日志末尾的分隔符 (默认为换行符 "\n")
        std::string_view separator{"\n"};

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(
                std::back_inserter(target),
                "LogMsgConfig(date={}, time={}, tid={}, header={}, level={})",
                gen_date, gen_time, gen_thread_id, out_header, out_level
            );
        }
    };

    /**
     * @brief Logger 核心配置
     */
    struct LoggerConfig {
        /// @brief 消费者工作线程数 (0 表示单线程同步模式，>0 为后台异步消费者线程模式)
        u32 consumer_count{1};
        /// @brief 每次从队列中批量拉取消费的最大消息数
        u32 fetch_message_count_max{128};
        /// @brief 是否启用背压机制 (队列积压过大时阻塞生产者协助消化)
        bool enable_back_pressure{false};
        /// @brief 背压触发倍率阈值
        u32 back_pressure_multiply{4};
        /// @brief 最大允许缓存的消息总数 (超出时触发丢弃一半的最老消息保护策略)
        u32 maximum_message_count{100'000};

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(
                std::back_inserter(target),
                "LoggerConfig(consumers={}, fetch_max={}, max_msgs={})",
                consumer_count, fetch_message_count_max, maximum_message_count
            );
        }
    };

    /**
     * @brief LogFactory 实例配置
     */
    struct LogFactoryConfig {
        using LevelKeepFn = bool (*)(int);

        /// @brief 默认日志级别
        int def_level{static_cast<int>(LogLevel::Info)};
        /// @brief 快速剪枝过滤函数 (返回 false 直接在工厂端丢弃，跳过字符串构造)
        LevelKeepFn level_should_keep{nullptr};
        /// @brief 工厂模块标识头 (例如 "Network", "Database")
        std::string_view header{""};
        /// @brief 默认消息格式配置
        LogMsgConfig msg{};

        LogFactoryConfig() = default;
        LogFactoryConfig(
            std::string_view iheader,
            int idef_level = static_cast<int>(LogLevel::Info),
            LevelKeepFn ilevel_should_keep = nullptr,
            const LogMsgConfig& msg_cfg = LogMsgConfig{}
        ) : def_level(idef_level)
          , level_should_keep(ilevel_should_keep)
          , header(iheader)
          , msg(msg_cfg) {}

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(
                std::back_inserter(target),
                "LogFactoryConfig(header=\"{}\", def_level={})",
                header, def_level
            );
        }
    };

} // namespace alib6::log
