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
#include <source_location>
#include <climits>

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
    concept CanLimitLog = requires(T&&t,size_t & estimated_size,size_t limit){
        t.limit_log(estimated_size,limit);
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
        size_t max_length { std::variant_npos };
        void limit_log(size_t & est,size_t mx){
            max_length = mx;
            estimated_length = &est;
        }

        void write_to_log(std::pmr::string & target){
            constexpr std::string_view s_bin = "01"; 
            constexpr std::string_view s_oct = "01234567"; 
            constexpr std::string_view s_hex_lower = "0123456789abcdef"; 
            constexpr std::string_view s_hex_upper = "0123456789ABCDEF";
            std::string_view s_hex = config.capital ? s_hex_upper : s_hex_lower;

            if(estimated_length){
                size_t bytes_per_unit = 1;
                if(config.fmt == Bin) bytes_per_unit = 8;
                else if(config.fmt == Hex) bytes_per_unit = 2;
                else if(config.fmt == Oct) bytes_per_unit = 3;

                *estimated_length = size * bytes_per_unit;
                if(config.split_when > 0){
                    *estimated_length += (*estimated_length / config.split_when) * config.split_str.size();
                }
            }

            size_t current_len = 0;
            size_t cycle_len = 0;
            unsigned char * cursor = (unsigned char*)data;
            size_t processed_bytes = 0;

            auto check_split = [&]() {
                if (config.split_when > 0 && ++cycle_len >= config.split_when){
                    cycle_len = 0;
                    for(char c : config.split_str){
                        if(current_len < max_length){
                            target.push_back(c);
                            current_len++;
                        }
                    }
                }
            };

            while(processed_bytes < size && current_len < max_length){
                unsigned char ch = *cursor;

                switch(config.fmt){
                    case Hex: {
                        target.push_back(s_hex[(ch >> 4) & 0x0F]);
                        current_len++;
                        check_split();

                        if(current_len < max_length) {
                            target.push_back(s_hex[ch & 0x0F]);
                            current_len++;
                            check_split();
                        }
                        break;
                    }
                    case Oct: {
                        unsigned char oct_parts[3] = {
                            (unsigned char)((ch >> 6) & 0x03),
                            (unsigned char)((ch >> 3) & 0x07),
                            (unsigned char)(ch & 0x07)
                        };
                        for(int i=0; i<3; ++i){
                            if(current_len < max_length){
                                target.push_back(s_oct[oct_parts[i]]);
                                current_len++;
                                check_split();
                            }
                        }
                        break;
                    }
                    case Bin: {
                        for(int i = CHAR_BIT - 1; i >= 0; --i){
                            if(current_len < max_length){
                                target.push_back(s_bin[(ch >> i) & 1]);
                                current_len++;
                                check_split();
                            }
                        }
                        break;
                    }
                }
                /// @note 实现Base32/Base64的时候注意这里要按照要求加上去
                cursor++;
                processed_bytes++;
            }
        }

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