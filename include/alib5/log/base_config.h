/**
 * @file base_config.h
 * @brief Configuration structures for the logging system. / 各种配置文件
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @version 0.1
 * @date 2026/06/18
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 *
 * @start-date 2025/11/27
 */
#ifndef ALOG_BASECONFIG_INCLUDED
#define ALOG_BASECONFIG_INCLUDED
#include <alib5/autil.h>

namespace alib5{
    /**
     * @brief Maximum number of custom tags per log entry.
     * @details Exceeding this count raises an error in debug builds and is silently ignored in release builds.
     * @par Original Comment:
     * 控制自定义tag的数量，当插入数量大于这个数字时debug模式下会报错，release模式下忽略，所以注意一条日志别插入太多tag
     */
    constexpr uint32_t log_custom_tag_count = 8;

    /**
     * @brief User-defined tag attached to a log message at a specific character position.
     * @par Original Comment:
     * 自定义的Tag
     */
    struct ALIB5_API LogCustomTag{
        /// @brief Insertion position stored as 1-based offset; 0 means invalid. 1 means before the 0th character. / 插入的位置，1 表示第0个字符前面，0表示无效
        /// @details Uses uint64_t for alignment instead of size_t. / 为了对齐，还是不准备使用size_t了
        uint64_t pos {0};
        /// @brief 16-bit category identifier. / 用于标识的种类id
        uint64_t category : 16;
        /// @brief 48-bit payload carrying the actual tag data. / 具体的信息
        uint64_t payload  : 48;

        /**
         * @brief Sets the insertion position from a 0-based offset.
         * @par Original Comment:
         * 正常人的设置方式，0表示第0个字符前面
         */
        inline void set(uint64_t p){pos = p + 1;}
        /**
         * @brief Retrieves the 0-based insertion offset; returns npos-like value on error.
         * @par Original Comment:
         * 获取位置，错误的时候返回npos也不错
         */
        inline uint64_t get(){return pos - 1;}
        /**
         * @brief Checks whether this tag carries a valid position.
         * @par Original Comment:
         * 获取当前是否valid
         */
        inline bool valid(){return pos != 0;}
    };

    /**
     * @brief Per-message detail controls used by LogFactory to govern extra information output.
     * @par Original Comment:
     * 日志消息细节控制，用在LogFactory中，控制额外信息的输出
     */
    struct ALIB5_API LogMsgConfig{
        /**
         * @brief Function pointer type mapping a numeric level to its display string.
         * @par Original Comment:
         * 可以通过这个形式的函数自定义某个数字对应的level显示
         */
        using LevelCastFn = std::string_view (*)(int);

        /// @brief When true, all extra-information generation below is skipped. Defaults to false. / 是否禁用额外信息生成，一旦为true，下面的额外信息也都没有意义了，默认为false
        bool disable_extra_information;

        /// @brief Whether to generate thread id, e.g. [TID424325]. Defaults to false. / 是否生成线程id,eg [TID424325]，默认为false
        bool gen_thread_id;
        /// @brief Whether to generate elapsed runtime, e.g. [999.12ms]. Defaults to true. / 是否生成运行时间,eg [999.12ms]，默认为true
        bool gen_time;
        /// @brief Whether to generate current date, e.g. [25-10-1 09:02:02]. Defaults to true. / 是否生成当前日期,eg [25-10-1 09:02:02]，默认为true
        bool gen_date;

        /// @brief Whether to print the LogFactory "head", e.g. "Test". Defaults to true. / 是否输出LogFactory的"head",eg "Test"，默认为 true
        bool out_header;
        /// @brief Whether to print the current log level. Defaults to true. / 是否输出当前日志级别，默认为 true
        bool out_level;
        /// @brief Function used to cast level numbers to strings; defaults to default_level_cast. / 用来转换level的函数，默认为 default_level_cast
        LevelCastFn level_cast;

        /// @brief Trailing separator appended to each log entry. / SEPARATOR,每条日志末尾的信息
        std::string_view separator;

        /**
         * @brief Default level caster that adapts to the LogLevel enum.
         * @par Original Comment:
         * 默认的level cast,适配LogLevel这个enum
         */
        static std::string_view default_level_cast(int level_in);

        /**
         * @brief Constructs the config with default values.
         * @par Original Comment:
         * 这个初始化了默认值
         */
        LogMsgConfig();
    };

    /**
     * @brief Configuration for the Logger core.
     * @par Original Comment:
     * Logger的配置文件
     */
    struct ALIB5_API LoggerConfig{
    public:
        /// @brief Number of consumer threads; 0 enables sync mode, otherwise async. Defaults to 1. / Consumer线程数量，0表示启动sync模式，否则为async，默认为1
        unsigned int consumer_count;
        /// @brief Maximum messages fetched per fetch_messages call; defaults to consumer_message_default_count. / fetch_message一次性能取出信息的最大值，默认为 consumer_message_default_count
        unsigned int fetch_message_count_max;
        /// @brief Whether back-pressure mode is enabled. Defaults to false (back-pressure causes message reordering); when false the fields below are inert. / 是否开启背压模式，默认为false（背压会出现消息不同步），如果为false，下面的内容失效
        bool enable_back_pressure;
        /**
         * @brief Back-pressure multiplier; threshold = multiplier * fetch max * consumer count. Defaults to 4.
         * @par Original Comment:
         * 背压模式的阈值，阈值 = 下面的倍率 * 一次性能取出最大值 * 消费者数量，默认为 4
         * @note  其实背压下相当于主线程也变成了一个消费者，每次push_message执行一次fetch
         */
        unsigned int back_pressure_multiply;
        /// @brief Maximum cached messages; exceeding this triggers a harsh drop-half policy. / 最大能缓存的消息数量,超过时再加入新的消息将会执行一个残酷的策略(drop half)
        unsigned int maximum_message_count;

        /**
         * @brief Constructs the default configuration.
         * @par Original Comment:
         * 构造默认配置
         */
        LoggerConfig();
    };

    /**
     * @brief Configuration for LogFactory.
     * @par Original Comment:
     * LogFactory配置
     */
    struct ALIB5_API LogFactoryConfig{
        /**
         * @brief Function pointer used to early-prune calls at the LogFactory stage.
         * @par Original Comment:
         * 这个用于在LogFactory阶段就直接pass掉调用，从而进一步剪枝叶
         * @param[in] level 当前日志的级别
         * @return true表示保留日志，false表示丢弃日志
         */
        using LevelKeepFn = bool(*)(int);

        /// @brief Default level value selected. / 默认选择的level数值
        int def_level;
        /// @brief Early-pruning function; defaults to null. / 用来提前剪枝的函数，默认为空
        LevelKeepFn level_should_keep;
        /// @brief Cached header; LogFactory reprocesses this via Logger::register_header to prevent dangling. / 缓存的header，LogFactory回对这个再处理，使用Logger::register_header来防止悬垂
        std::string_view header;
        /// @brief Default message configuration. / 默认的消息配置信息
        LogMsgConfig msg;

        /**
         * @brief Constructs the config with default values.
         * @par Original Comment:
         * 构造默认数值
         */
        LogFactoryConfig(
            std::string_view iheader = "",
            int idef_level = 0,
            LevelKeepFn ilevel_should_keep = nullptr,
            const LogMsgConfig & msg_cfg = LogMsgConfig()
        );
    };
}

#endif
