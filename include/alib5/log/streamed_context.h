/**
 * @file streamed_context.h
 * @brief StreamedContext template providing << chaining for log output with compile-time injection of write_to_log. / 流式输出处控制，主持write_to_log(类函数&全局函数)编译期注入
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @version 0.1
 * @date 2026/06/18
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 *
 * @start-date 2025/11/27
 */
#ifndef ALOG_STREAMED_CONTEXT_INCLUDED
#define ALOG_STREAMED_CONTEXT_INCLUDED
#include <alib5/autil.h>
#include <alib5/log/base_msg.h>
#include <alib5/log/base_config.h>
#include <alib5/log/manipulator.h>
#include <alib5/adebug.h>
#include <format>

#ifndef ALIB_DISABLE_GLM_EXTENSIONS
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif

namespace alib5{
    /// @brief Concept satisfied by any std::formattable type (including nested containers, compiler permitting). 基础类型使用std::format，包含嵌套容器（如果你的编译器不支持需要自己实现下面的canforward）
    template<class T> concept GoUniversal = std::formattable<T,char>;
    /// @brief Concept satisfied by types that can serialize to a pmr string via a member or free write_to_log; member function takes priority. 对于能够forward的对象进行forward。forward优先级：外部 > 成员函数
    template<class T> concept CanForward = requires(T && t,std::pmr::string & target){
        t.write_to_log(target);
    } || requires(T && t,std::pmr::string & target){
        write_to_log(target,t);
    };
    /// @brief Concept satisfied by types exposing a manipulate(LogMsgConfig&) method. LogMsgConfig的操作器，满足这个接口就可以直接处理信息了
    template<class T> concept CanManipulate = requires(T && t,LogMsgConfig & c){
        t.manipulate(c);
    };
    /// @brief Concept satisfied by types that can take ownership of a StreamedContext via self_forward. 对象可以接管 StreamedContext 的控制权
    template<class T,class Context>
    concept CanSelfForward = requires(T && t,Context && ctx){
        { std::forward<T>(t).self_forward(std::move(ctx)) } -> std::same_as<Context&&>;
    };

    /**
     * @brief Move-only streaming context that accumulates formatted output and uploads it to a LogFactory on termination; supports manipulators, format strings, omit/rate limiting, tags and self-forwarding objects.
     *
     * @par Original Comment:
     * 流式输出核心
     * @tparam LogFactory 类CRTP写法与LogFactory解耦，理论上只能接上LogFactory
     */
    template<class LogFactory> struct StreamedContext{
        /// @brief Reference to the owning LogFactory. 持有的LogFactory对象
        LogFactory & factory;
        /// @brief Per-context string buffer that will be forwarded to the Logger. 流式输出内部缓存的字符串，考虑到刚好可以直接转发给Logger，因此每个context一份
        std::pmr::string cache_str;
        /// @brief Numeric log level for this context. 日志级别
        int level;
        /// @brief Whether this context will actually compose and upload; flipped to false when the level fails the LogFactory's keep predicate. 当前Context是否是启用的，如果Level不满足LogFactory需求其会被标记为invalid，进而不组合&上传日志
        bool context_valid;
        /// @brief Guards against re-use; any operation after upload panics in both debug and release. 当前临时对象是否已经被使用，如果被使用，则会在upload阶段直接panic，无论debug/release
        bool context_used;
        /// @brief Active format string; cleared after use. 格式化字符串，用后就清除
        std::string_view fmt_str;
        /// @brief Whether the current format string is temporary (auto-cleared after one use). 格式化是否是临时的
        bool fmt_tmp;
        /// @brief RAII lock guard for fmt_tmp during nested formatting. 对于内部格式化函数,可能需要锁住整个fmt_tmp
        bool fmt_locked;
        /// @brief Per-context message configuration. 配置信息
        LogMsgConfig msg_cfg;
        /// @brief Tags attached to this context. TAG信息
        std::pmr::vector<LogCustomTag> tags;

        /// @brief RAII helper that sets fmt_locked on construction and clears it on destruction.
        struct FMTLock{
            bool & v;
            FMTLock(bool & x):v(x){
                v = true;
            }
            ~FMTLock(){
                v = false;
            }
        };

        // 禁止拷贝，允许移动
        StreamedContext(const StreamedContext&) = delete;
        StreamedContext& operator=(const StreamedContext&) = delete;
        StreamedContext(StreamedContext&&) = default;
        StreamedContext& operator=(StreamedContext&&) = default;

        /// @brief Acquires an RAII lock guarding fmt_tmp.
        auto lock_fmt(){
            return FMTLock(fmt_locked);
        }

        /// @brief Clears the format string if it is temporary and not currently locked.
        auto try_restore_fmt(){
            if(fmt_tmp && !fmt_locked){
                fmt_str = "";
            }
        }

        /// @brief Constructs the context with the given level, factory and validity flag; allocates cache_str and tags from the factory's pools. 初始化字符串用的内存池以及level
        inline StreamedContext(int level,LogFactory & fac,bool valid = true)
        :factory(fac)
        ,cache_str(fac.get_msg_str_alloc())
        ,msg_cfg(fac.get_msg_config())
        ,tags(fac.get_tag_alloc()){
            this->level = level;
            fmt_str = "";
            fmt_tmp = false;
            context_used = false;
            context_valid = valid;
            fmt_locked = false;
        }

        /// @brief Uploads the accumulated message to the factory; the context is invalid afterwards.
        /// @return 是否上传成功
        /// @note   StreamedContext设计出来就是用于局部构造的，因此upload后就失效了
        inline bool upload() && {
            [[unlikely]] panic_if(context_used,"The context has been used!");
            context_used = true;
            // 拒绝上传空的日志
            if(context_valid && !cache_str.empty())
                return factory.log_pmr(level,cache_str,msg_cfg,tags);
            return false;
        }

        /// @brief Writes pre-built format args using the current format string (or "{}" when empty). 直接写入format args
        template<class _Context, class... _InternalArgs>
        StreamedContext&& operator<<(std::__format::_Arg_store<_Context, _InternalArgs...> && ctx) {
            if(!context_valid) return std::move(*this);
            std::vformat_to(std::back_inserter(cache_str),
                        fmt_str.empty() ? "{}" : fmt_str,
                        ctx);

            try_restore_fmt();
            return std::move(*this);
        }

        /// @brief Writes val, truncating to max_length and appending the omit placeholder when the limit is exceeded. 省略处理
        template<class T>
        StreamedContext&& operator<<(log_omit<T> && val) && {
            if(!context_valid)return std::move(*this);

            constexpr bool limited = CanLimitLog<std::decay_t<T>>;
            size_t estimated_length = 0;
            if constexpr(limited){
                std::forward<T>(val.v).limit_log(estimated_length,val.max_length);
            }

            // 记录当前长度
            size_t clen = cache_str.size();
            std::move(*this) << std::forward<T>(val.v);
            // 记录现在长度
            size_t clen2 = 0;
            if constexpr(limited){
                clen2 = std::max(cache_str.size(),clen + estimated_length);
            }else{
                clen2 = cache_str.size();
            }
            // 进行截断
            if(clen + val.max_length < clen2){
                cache_str.resize(clen + val.max_length);
                if(val.need_fmt){
                    std::format_to(
                        std::back_inserter(cache_str),
                        std::dynamic_format(val.omit_str),
                        clen2 - clen - val.max_length
                    );
                }else cache_str.append(val.omit_str);
            }
            return std::move(*this);
        }

        /// @brief Generic write helper that avoids << matching too greedily. 防止<<通用匹配过于通用而设计的
        template<class T>
        StreamedContext&& write(T && t) && {
            if(!context_valid)return std::move(*this);
            if(fmt_str.empty()){
                if constexpr(IsStringLike<T>) {
                    cache_str.append(std::forward<T>(t));
                }else{
                    std::format_to(std::back_inserter(cache_str),"{}",t);
                }
            }else{
                std::vformat_to(std::back_inserter(cache_str),fmt_str.data(),
                    std::make_format_args(t)
                );
                try_restore_fmt();
            }
            return std::move(*this);
        }

        /// @brief Applies a manipulator that satisfies CanManipulate to the per-context config. 注入式的manipulator
        template<CanManipulate T>
        StreamedContext&& operator<<(T && t) && {
            if(!context_valid)return std::move(*this);
            t.manipulate(msg_cfg);
            return std::move(*this);
        }

        /// @brief Formats a GoUniversal value via the current format string (or "{}"). 格式化基础类型
        template<GoUniversal T>
        StreamedContext&& operator<<(T && t) && {
            return std::move(*this).template write<T>(std::forward<T>(t));
        }

        /// @brief Forwards a CanForward value by invoking its write_to_log (member or free); the free function enables non-intrusive override. 格式化可转发类型
        template<CanForward T>
        StreamedContext&& operator<<(T && t) && {
            if(!context_valid)return std::move(*this);
            // 是的，孩子们，你可以通过这个实现非侵入式的覆写了
            if constexpr(requires(T&&t,std::pmr::string&val){write_to_log(val,t);}){
                write_to_log(cache_str,t);
            }else{
                t.write_to_log(cache_str);
            }
            return std::move(*this);
        }

        /// @brief No-op terminator that returns the context unchanged. 直接返回自己
        inline StreamedContext&& operator<<(log_nop fn) && {
            return std::move(*this);
        }

        /// @brief Removes count characters from the buffer and pops any tags whose position now exceeds the buffer length. 移除字符,同时处理溢出的tag
        inline StreamedContext&& operator<<(log_erase fn) && {
            if(fn.count >= cache_str.size()){
                tags.clear();
                cache_str.clear();
            }else{
                // 因为tag.pos具有线性递增的特性
                cache_str.resize(cache_str.size() - fn.count);
                while(!tags.empty() && tags.back().get() > cache_str.size()){
                    tags.pop_back();
                }
            }
            return std::move(*this);
        }

        /// @brief Terminates the log via the endlog callback. 支持endlog终止日志
        inline bool operator<<(EndLogFn fn) && {
            return std::move(*this).upload();
        }

        /// @brief Terminates the log via std::endl. 支持std::endl终止日志
        inline bool operator<<(std::ostream& (*manip)(std::ostream&)) && {
            return std::move(*this).upload();
        }

        // 孩子们，你觉得下面的terminate method还是人类吗？

        /// 这里支持的是fls
        /// @brief Terminates the log via a LogEnd value (fls). YetAnotherTerminateMethod
        inline bool operator<<(LogEnd) && {
            return std::move(*this).upload();
        }

        /// @brief Terminates the log via the pipe operator with a LogEnd value. YetAnotherAnotherTerminateMethod
        inline bool operator|(LogEnd) && {
            return std::move(*this).upload();
        }

        /// @brief Appends a user-defined tag at the current buffer position. 支持插入自定义的信息
        inline StreamedContext&& operator<<(const log_tag & t) && {
            if(!context_valid)return std::move(*this);
            LogCustomTag & tag = tags.emplace_back();
            tag.category  = t.category;
            tag.payload = t.payload;
            tag.set(cache_str.size());
            return std::move(*this);
        }

        /// @brief Hands control to a self-forwarding object for complex extension. 支持 SelfForward 接口，对象接管 Context 进行复杂扩展
        template<CanSelfForward<StreamedContext> T>
        StreamedContext&& operator<<(T && t) && {
            if(!context_valid)return std::move(*this);
            return std::forward<T>(t).self_forward(std::move(*this));
        }

        /// @brief Sets a persistent format string (caller must ensure it does not dangle within the statement). 支持设置格式化，需要保证局部不悬垂（应该没人会在一句话就把数据删除了吧）
        inline StreamedContext&& operator<<(const log_fmt & fmt) && {
            if(!context_valid)return std::move(*this);
            fmt_str = fmt.fmt_str.data();
            fmt_tmp = false;
            return std::move(*this);
        }

        /// @brief Sets a temporary format string (auto-cleared after one use). 支持设置格式化，需要保证局部不悬垂（应该没人会在一句话就把数据删除了吧）
        inline StreamedContext&& operator<<(const log_tfmt & fmt) && {
            if(!context_valid)return std::move(*this);
            fmt_str = fmt.fmt_str.data();
            fmt_tmp = true;
            return std::move(*this);
        }

        #ifndef ALIB_DISABLE_GLM_EXTENSIONS
        /// @brief Formats a glm vector by treating its components as a span. 格式化glm的向量
        template<int N,class T,enum glm::qualifier Q>
            inline StreamedContext&& operator<<(const glm::vec<N,T,Q> & v) && {
            if(!context_valid)return std::move(*this);
            std::span<const T,N> value(glm::value_ptr(v),N);
            return std::move(*this) << value;
        }
        /// @brief Formats a glm matrix row by row, wrapped in braces. 格式化glm的矩阵
        template<int M,int N,class T,enum glm::qualifier Q>
            inline StreamedContext&& operator<<(const glm::mat<M,N,T,Q> & v) && {
            if(!context_valid)return std::move(*this);
            std::span<const T,M*N> data (glm::value_ptr(v),M*N);
            cache_str.append("{");
            {
                auto lock = lock_fmt();
                for(int m = 0;m < M;++m){
                    std::move(*this) << data.subspan(m*N,N);
                    if(m+1 != M)cache_str.append(" , ");
                }
            }
            cache_str.append("}");

            try_restore_fmt();
            return std::move(*this);
        }
        /// @brief Formats a glm quaternion by treating its components as a 4-element span. 格式化glm的四元数
        template<class T,enum glm::qualifier Q>
            inline StreamedContext&& operator<<(const glm::qua<T,Q> & v) && {
            if(!context_valid)return std::move(*this);
            std::span<const T,4> data (glm::value_ptr(v),4);
            return std::move(*this) << data;
        }
        #endif
    };

    /// @brief Bundles an arbitrary argument list so it can be forwarded into a StreamedContext as a single unit. 对你的参数列表进行简单的封装提高可用性
    template<typename... Ts>
    struct CoupledArgs {
        /// @brief Stored argument tuple. storage
        std::tuple<Ts...> storage;

        /// @brief Constructs from forwarded arguments.
        template<typename... Args>
        explicit CoupledArgs(Args&&... args)
            : storage(std::forward<Args>(args)...) {}

        /// @brief Self-forwards by applying the stored tuple as format args into ctx.
        template<class Context>
        Context&& self_forward(Context&& ctx) && {
            return std::apply([&ctx](auto&... args) mutable -> Context&& {
                return std::move(ctx) << std::make_format_args(args...);
            }, storage);
        }
    };

    /// @brief Constructs a CoupledArgs bundle from the given arguments. 生成couple
    template<typename... Args>
    auto couple(Args&&... args) {
        return CoupledArgs<std::decay_t<Args>...>(std::forward<Args>(args)...);
    }
}

#endif
