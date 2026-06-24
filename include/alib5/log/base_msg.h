/**
 * @file base_msg.h
 * @brief LogMsg structure carrying one log entry through the pipeline. / 消息的基础架构
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @version 0.1
 * @date 2026/06/20
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 *
 * @start-date 2025/11/17
 */
#ifndef ALOG_LOGMSG_INCLUDED
#define ALOG_LOGMSG_INCLUDED
#include <alib5/log/base_config.h>

namespace alib5{
    /**
     * @brief A single log message flowing through the pipeline.
     * @par Original Comment:
     * 日志的每一条消息
     */
    struct ALIB5_API LogMsg{
    private:
        friend class Logger;
        /// @brief Marks whether the message is acceptable; set during the filter stage. / 标识日志是否可用，会在filter阶段标记
        bool m_nice_one { false };
        /// @brief Marks whether the composed string has already been generated to avoid rework. / 标识当前日志是否已经生成了composed str从而减少构造
        bool generated { false };
    public:
        /// @brief Header string_view pointing into the Logger's header pool; guaranteed not to dangle. / 指向Logger中的header pool,保证不悬垂
        std::string_view header { "" };
        /// @brief The main composed body of the log entry. / 最终合成的日志主要部分
        std::pmr::string body;
        /// @brief Per-message config; using a value (not pointer) to avoid dangling-pointer safety issues. / 日志指向的配置，目前觉得使用指针会带来安全风险，因此尝试一下const结构体
        LogMsgConfig cfg;
        /// @brief Severity level of this entry. / 这条日志的级别
        int level { 0 };

        //// 附加生成信息 ////
        /// @brief Thread id. / 线程id
        uint64_t thread_id { 0 };
        /// @brief Timestamp captured by the producer. / producer生成的时间戳
        double timestamp { 0 };
        /// @brief User-defined tags. / 用户自定义的tag
        std::pmr::vector<LogCustomTag> tags;

        //// 缓冲 ////
        static std::string& sdate();
        static std::string& scomposed();

        /**
         * @brief Constructs core content with allocators and config.
         * @par Original Comment:
         * 构造核心内容
         */
        LogMsg(const std::pmr::polymorphic_allocator<char> &__a,const std::pmr::polymorphic_allocator<LogCustomTag> & __tg,const LogMsgConfig & c);
        /**
         * @brief Constructs an empty placeholder message.
         * @par Original Comment:
         * 构造空的数据，用于占位
         */
        LogMsg(){}

        /**
         * @brief Producer-side setup: fills in timestamp and thread id.
         * @par Original Comment:
         * Producer构造基础信息，如timestamp和tid
         */
        void build_on_producer(const timespec & start);
        /**
         * @brief Consumer-side setup: composes the string and stores it in this structure.
         * @note  由于这个函数使用的是static threadlocal的变量
         *        因此必须保证对日志信息主遍历而不是对targets
         * @par Original Comment:
         * Consumer合成字符串存储到当前结构
         * @note  由于这个函数使用的是static threadlocal的变量
         *        因此必须保证对日志信息主遍历而不是对targets
         */
        void build_on_consumer();

        /**
         * @brief Generates the composed string lazily; first call builds it, later calls return the cached one when no other logs are inserted in the same context. Note: a trailing newline is appended; trim it if unwanted.
         * @par Original Comment:
         * 生成组合数据，懒加载，在无其他日志插入的context中，第一次调用会构造，之后之前返回构造了的。注意，最后面有一个换行符，不想要可以截取
         */
        std::string_view gen_composed();

        /**
         * @brief Move-constructs the log message; mainly moves pmr objects which should keep a consistent allocator.
         * @par Original Comment:
         * 移动构造日志，主要处理为移动构造pmr对象，pmr对象最好保持allocator一致
         */
        void move_msg(LogMsg&& msg);
        /**
         * @brief Move assignment.
         * @par Original Comment:
         * 移动构造
         */
        inline LogMsg& operator=(LogMsg&& msg){
            move_msg(std::forward<LogMsg>(msg));
            return *this;
        }
        /**
         * @brief Move constructor.
         * @par Original Comment:
         *  移动构造
         */
        inline LogMsg(LogMsg&& msg){
            move_msg(std::forward<LogMsg>(msg));
        }
        /**
         * @brief Debug helper: clears the generated flag.
         * @par Original Comment:
         * 调试使用，清除flag
         */
        inline void clear_gen_flag(){
            generated = false;
        }
    };
}

#endif
