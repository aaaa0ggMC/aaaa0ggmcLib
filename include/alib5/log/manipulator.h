/**
 * @file manipulator.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些基本的操作符
 * @version 0.1
 * @date 2026/03/19
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef ALOG2_MANIP_INCLUDED
#define ALOG2_MANIP_INCLUDED
#include <alib5/log/base_config.h>
#include <alib5/autil.h>
#include <alib5/adebug.h>
#include <alib5/aclock.h>
#include <source_location>
#include <list>

namespace alib5{
    /// @brief 仅用来识别日志终止
    struct ALIB5_API LogEnd{};
    typedef void (*EndLogFn)(LogEnd);
    /// @brief 流式输出日志终止表示
    inline void ALIB5_API endlog(LogEnd){}
    /// @brief 另一种方式
    constexpr LogEnd fls = {};
    /// @brief 默认省略符号
    constexpr std::string_view default_omit_str = "...[+{} bytes]";

    /// @brief 表示你的manipulator等类似对象支持中途截断从而减少IO输出
    template<class T>
    concept CanLimitLog = requires(T&&t,size_t & estimated_size,size_t & max_length){
        t.limit_log(estimated_size,max_length);
    };

    /// @brief stacktrace支持
    struct ALIB5_API log_stacktrace{
        size_t skip_depth;
        bool skip_prompt;
        std::string_view skip_prompt_str;
        bool auto_newline;

        log_stacktrace(size_t skip_depth = 2,bool skip_prompt = true,std::string_view skip_prompt_str = "c++",bool auto_newline = true)
        :skip_depth(skip_depth)
        ,skip_prompt(skip_prompt)
        ,skip_prompt_str(skip_prompt_str)
        ,auto_newline(auto_newline){}

        void write_to_log(std::pmr::string & str);
    };

    /// @brief 限制单条日志输出次数
    struct ALIB5_API log_rate{
        struct ALIB5_API Info{
            enum Type{
                Rate,
                Once
            };
            Type type { Rate };
            std::atomic<bool> printed;
            std::atomic<double> last_all;
            double times_per_second { -1 };
            alib5::Clock clk;

            auto& set_type(Type t){
                type = t;
                return *this;
            }

            auto & set_rate(double times_per_sec){
                times_per_second = times_per_sec;
                return *this;
            }

            bool check();
        };

        struct ALIB5_API context{
            std::pmr::list<Info> info;
            // 不直接使用是为了防止mapping rehash
            std::pmr::unordered_map<size_t,Info*> mapping;
            std::mutex mutex;

            context(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE)
            :info(allocator)
            ,mapping(allocator){}

            Info & get(size_t id){
                std::lock_guard<std::mutex> lock(mutex);
                auto it = mapping.find(id);
                if(it == mapping.end()){
                    auto & target = info.emplace_back();
                    mapping.emplace(id,&target);
                    return target;
                }else return *it->second;
            }            
        };
        Info& info;

        log_rate(size_t id,context & context)
        :info(context.get(id)){}

        log_rate(Info & i_info)
        :info(i_info){}

        template<class T>
        auto self_forward(T && context) -> decltype(std::forward<T>(context)) {
            if(!info.check())context.context_valid = false;
            return std::forward<T>(context);
        }
    };

    /// @brief 临时存储的东西
    template<class T>
    struct ALIB5_API log_omit{
        T v;
        size_t max_length;
        std::string_view omit_str;
        bool need_fmt;

        template<class R>
        log_omit(R && iv,size_t max_count,std::string_view omit = default_omit_str,bool ineed_fmt = true)
        :v(std::forward<R>(iv))
        ,max_length(max_count)
        ,omit_str(omit)
        ,need_fmt(ineed_fmt){}

        log_omit(const log_omit & ) = delete;
        log_omit& operator=(const log_omit&) = delete;
    };
    template<class R,class... Args>
    log_omit(R && iv,Args&&...) -> log_omit<R>;

    /// @brief 使用数字呈现数据,不仅仅是hex
    struct ALIB5_API log_bin{
        enum Format{
            Bin = 1,
            Oct = 3,
            Hex = 4
        };

        struct cfg{
            Format fmt;
            size_t split_when;
            std::string_view split_str;
            // 是否大写
            bool capital;

            cfg(){
                // 默认是16进制
                fmt = Hex;
                // hex是两个字符一个空格
                split_when = 2;
                split_str = " ";
                capital = false;
            }
        };

        cfg config;
        const void * data;
        size_t size;        

        size_t* estimated_length { nullptr };
        size_t * parent_ml { nullptr };
        size_t max_length { std::variant_npos };
        void limit_log(size_t & est,size_t & ml){
            max_length = ml;
            estimated_length = &est;
            parent_ml = &ml;
        }

        void write_to_log(std::pmr::string & target);

        template<class T>
        log_bin(T && val,cfg c = cfg()):data(&val),size(sizeof(val)),config(c){}
        template<class T>
        log_bin(T * val,size_t s,cfg c = cfg()):data(val),size(s),config(c){}
        template<class T>
        log_bin(std::span<T> list,cfg c = cfg()):data(list.data()),size(sizeof(T) * list.size()),config(c){}
        template<class T>
        log_bin(std::basic_string_view<T> list,cfg c = cfg()):data(list.data()),size(sizeof(T) * list.size()),config(c){}
    };

    

    struct ALIB5_API log_source{
        /// @brief 缓存的路径
        std::source_location loc;
        /// @brief 是否保存完整路径，默认true
        bool keep_full;
        
        log_source(bool kf = true,std::source_location cloc = std::source_location::current()):loc(cloc),keep_full{kf}{}

        template<class T> void write_to_log(T & str){
            std::string_view p = loc.file_name();
            if(!keep_full){
                auto pos = p.find_last_of("/");
                if(pos != std::string_view::npos && pos+1 < p.size()){
                    p = p.substr(pos+1);
                }
            }
            std::format_to(std::back_inserter(str),"[{}:{}:{} {}]",p,loc.line(),loc.column(),loc.function_name());
        }
    };

    /// @brief format标识，nullptr清空fmt，长期保持
    /// @note  清空fmt会带来更快的格式化
    struct log_fmt{
        /// @brief 格式化字符串
        std::string_view fmt_str;

        log_fmt(std::string_view s = ""):fmt_str(s){}
    };

    /// @brief 临时的format标志，最好只用来输出基础数据类型
    /// @note  清空fmt会带来更快的格式化
    struct log_tfmt{
        /// @brief 格式化字符串
        std::string_view fmt_str;

        log_tfmt(std::string_view s = ""):fmt_str(s){}
    };

    /// @brief 单条日志设置是否展示header
    struct log_header{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_header(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.out_header = val;
        }
    };

    /// @brief 单条日志设置是否展示level
    struct log_level{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_level(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.out_level = val;
        }
    };

    /// @brief 单条日志设置是否展示date
    struct log_date{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_date(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.gen_date = val;
        }
    };

    /// @brief 单条日志设置是否展示level
    struct log_time{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_time(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.gen_time = val;
        }
    };

    /// @brief 单条日志设置是否展示thread_id
    struct log_tid{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_tid(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.gen_thread_id = val;
        }
    };
    
    /// @brief 直接忽略这条内容,用于编译期省略不错
    struct log_nop{};

    /// @brief 移除多少字符
    struct log_erase{
        size_t count;
        
        log_erase(size_t c):count(c){}
    };

    /// @brief 自定义的推送给target的manipulate
    struct log_tag{
        uint64_t category : 16;
        uint64_t payload : 48;

        inline constexpr log_tag(uint16_t cate_id,uint64_t payload){
            panic_debug(payload > 0xFFFFFFFFFFFFULL, "Payload value must be in range 0 - (2^48 - 1).");
            category = cate_id;
            this->payload = payload;
        }
    };
}

#endif