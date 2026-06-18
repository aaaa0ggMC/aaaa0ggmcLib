/**
 * @file base_mod.h
 * @brief Base module interfaces (LogTarget / LogFilter) for the logging pipeline. / 基础module接口
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @version 0.1
 * @date 2026/06/18
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 *
 * @start-date 2025/11/17
 */
#ifndef ALOG_MOD_INCLUDED
#define ALOG_MOD_INCLUDED
#include <alib5/log/base_msg.h>
#include <type_traits>

namespace alib5{
    /**
     * @brief Output sink interface: writes formatted log messages to a destination.
     * @par Original Comment:
     * 输出对象
     */
    struct ALIB5_API LogTarget{
        /// @brief Toggle used to enable/disable output; usually left untouched. / 用于toggle输出，一般不用管
        bool enabled;

        /**
         * @brief Toggles the enabled state and returns self for chaining.
         * @par Original Comment:
         * 切换状态
         */
        LogTarget& toggle(bool val){
            enabled = val;
            return *this;
        }

        /**
         * @brief Default-constructs an enabled target.
         * @par Original Comment:
         * 默认target就是enabled
         */
        inline LogTarget(){
            enabled = true;
        }

        /**
         * @brief Writes a message somewhere.
         * @note  如果节约内存修改logmsg也不是不行
         * @par Original Comment:
         * 往某个地方写入消息
         */
        virtual inline void write(
            LogMsg & msg
        ){}

        /**
         * @brief Flushes buffered data.
         * @par Original Comment:
         * 刷新数据，比如缓冲区啥的
         */
        virtual inline void flush(){}

        /**
         * @brief Closes IO resources.
         * @note  析构函数会自动close
         * @par Original Comment:
         * 关闭IO
         */
        virtual inline void close(){}

        /**
         * @brief Auto-closes resources on destruction.
         * @par Original Comment:
         * 自动关闭资源
         */
        virtual ~LogTarget(){
            flush();
            close();
        }
    };

    /**
     * @brief Log filter interface: supports rejecting or modifying log messages.
     * @par Original Comment:
     * 日志过滤器，支持日志拒绝，日志修改啥的
     */
    struct ALIB5_API LogFilter{
        /// @brief Indicates whether this filter is active. / 标示是否启用
        bool enabled;

        /**
         * @brief Toggles the enabled state and returns self for chaining.
         * @par Original Comment:
         * 切换状态
         */
        LogFilter& toggle(bool val){
            enabled = val;
            return *this;
        }

        /**
         * @brief Default-constructs an enabled filter.
         * @par Original Comment:
         * 默认启用日志
         */
        inline LogFilter(){
            enabled = true;
        }

        /**
         * @brief Attempts to filter a log message; may mutate it.
         * @return true:日志可以被处理，false:日志应该被抛弃
         * @par Original Comment:
         * 尝试过滤日志，可以修改
         * @return true:日志可以被处理，false:日志应该被抛弃
         */
        virtual inline bool filter(LogMsg & msg){
            return true;
        }

        /**
         * @brief Pre-filters a log message at push time before the body is built.
         * @param level_id     当前的输出级别
         * @param raw_message  日志原始body
         * @param cfg          当前的配置
         * @return true:日志可以被处理，false:日志应该被抛弃
         * @par Original Comment:
         * 尝试在push message时提前处理日志
         * @param level_id     当前的输出级别
         * @param raw_message  日志原始body
         * @param cfg          当前的配置
         * @return true:日志可以被处理，false:日志应该被抛弃
         */
        virtual inline bool pre_filter(
            int level_id,
            std::string_view raw_message,
            const LogMsgConfig & cfg
        ){
            return true;
        }

        /**
         * @brief Closes the filter; may release resources, though destruction handles it too.
         * @note  析构函数会自动close
         * @par Original Comment:
         * 关闭过滤器，也许你可以处理一些什么释放啥的，虽然析构也不是不行
         */
        virtual inline void close(){}

        /**
         * @brief Auto-closes the object on destruction.
         * @par Original Comment:
         * 自动close对象
         */
        virtual inline ~LogFilter(){
            close();
        }
    };

    /// @brief Concept requiring T to derive from LogTarget. / concept for LogTarget
    template<class T> concept IsLogTarget = std::is_base_of_v<LogTarget,T>;
    /// @brief Concept requiring T to derive from LogFilter. / concept for LogFilter
    template<class T> concept IsLogFilter = std::is_base_of_v<LogFilter,T>;
    /// @brief Concept satisfied when T is either a LogTarget or a LogFilter. / concept for LogMod
    template<class T> concept IsLogMod = IsLogTarget<T> || IsLogFilter<T>;

    /**
     * @brief Composite LogTarget that fans writes/flushes out to a tuple of targets.
     * @par Original Comment:
     * LogTargetGroup
     */
    template<IsLogTarget... Ts> struct ALIB5_API LogTargetGroup : public LogTarget{
        /// @brief Tuple type holding the wrapped targets. / 内部targets类型
        using targets_t = std::tuple<Ts...>;
        /// @brief The wrapped targets. / 内部targets
        targets_t targets;

        /// @brief Constructs the group from a moved tuple of targets. / 移动构造tuple
        LogTargetGroup(targets_t && t):targets(std::move(t)){}
        /// @brief Constructs the group from individual target arguments. / 从参数构造
        LogTargetGroup(Ts&&... args):targets(std::forward<Ts>(args)...){}

        /**
         * @brief Toggles the Nth target on/off.
         * @par Original Comment:
         * (implicit) toggleTarget
         */
        template<size_t N> inline void toggleTarget(bool val = true){
            std::get<N>(targets).enabled = val;
        }

        /**
         * @brief Toggles all targets on/off.
         * @par Original Comment:
         * (implicit) toggleAll
         */
        inline void toggleAll(bool val){
            std::apply([&val](auto &... t){
                ((t.enabled = val),...);
            },targets);
        }

        /// @brief Writes the message to every enabled target. / 写入所有target
        inline void write(LogMsg & msg) override {
            std::apply([&msg](auto &... t){
                ((t.enabled?t.write(msg):void()),...);
            },targets);
        }

        /// @brief Flushes every enabled target. / flush所有target
        inline void flush() override {
            std::apply([](auto &... t){
                ((t.enabled?t.flush():void()),...);
            },targets);
        }
    };


    /**
     * @brief Composite LogFilter that ANDs together the verdicts of multiple filters.
     * @par Original Comment:
     * LogFilterGroup
     */
    template<IsLogFilter... Ts> struct ALIB5_API LogFilterGroup : public LogFilter{
        /// @brief Tuple type holding the wrapped filters. / 内部filters类型
        using filters_t = std::tuple<Ts...>;
        /// @brief The wrapped filters. / 内部filters
        filters_t filters;

        /// @brief Constructs the group from a moved tuple of filters. / 移动构造tuple
        LogFilterGroup(filters_t && t):filters(std::move(t)){}
        /// @brief Constructs the group from individual filter arguments. / 从参数构造
        LogFilterGroup(Ts&&... args):filters(std::forward<Ts>(args)...){}

        /**
         * @brief Toggles the Nth filter on/off.
         * @par Original Comment:
         * (implicit) toggleFilter
         */
        template<size_t N> inline void toggleFilter(bool val = true){
            std::get<N>(filters).enabled = val;
        }

        /**
         * @brief Toggles all filters on/off.
         * @par Original Comment:
         * (implicit) toggleAll
         */
        inline void toggleAll(bool val){
            std::apply([&val](auto &... t){
                ((t.enabled = val),...);
            },filters);
        }

        /// @brief AND-combines the verdicts of every enabled filter. / 合并filter结果
        inline bool filter(LogMsg & msg) override{
            bool ret = true;

            std::apply([&ret,&msg](auto &... t){
                ((ret = ret && (t.enabled?t.filter(msg):true)),...);
            },filters);
            return ret;
        }

        /// @brief AND-combines the pre-filter verdicts of every enabled filter. / 合并pre_filter结果
        inline bool pre_filter(
            int level_id,
            std::string_view raw_message,
            const LogMsgConfig & cfg
        ) override{
            bool ret = true;
            std::apply([&ret,&level_id,&raw_message,&cfg](auto &... t){
                ((ret = ret && (t.enabled?t.pre_filter(level_id,raw_message,cfg):true)),...);
            },filters);
            return ret;
        }
    };
}

#endif
