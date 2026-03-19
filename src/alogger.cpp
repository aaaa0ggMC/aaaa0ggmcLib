#include <alib5/alogger.h>
#include <climits>
#include <stacktrace>

using namespace alib5;

thread_local std::string LogMsg::sdate;
thread_local std::string LogMsg::scomposed;
std::mutex lot::Console::console_lock;

void log_stacktrace::write_to_log(std::pmr::string & str){
    int index = -1;
    for(auto & entry : std::stacktrace::current()){
        index++;
        if(index < skip_depth)continue;
        if(entry.source_file().empty() && entry.source_line() == 0)continue;
        if(skip_prompt && 
           !skip_prompt_str.empty() && 
           entry.source_file().find(skip_prompt_str) != std::string::npos
        )continue; // skip c++ stacktrace

        str +=  "  ";
        str += ext::to_string<size_t,false>(index - skip_depth);
        str += "# ";
        str += entry.source_file();
        str += ":";
        str += ext::to_string<size_t,false>(entry.source_line());
        str += " at ";
        str += entry.description();
        if(auto_newline)str += "\n";
    }
}

void log_bin::write_to_log(std::pmr::string & target){
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

                target.push_back(s_hex[ch & 0x0F]);
                current_len++;
                check_split();
                break;
            }
            case Oct: {
                unsigned char oct_parts[3] = {
                    (unsigned char)((ch >> 6) & 0x03),
                    (unsigned char)((ch >> 3) & 0x07),
                    (unsigned char)(ch & 0x07)
                };
                for(int i=0; i<3; ++i){
                    target.push_back(s_oct[oct_parts[i]]);
                    current_len++;
                    check_split();
                }
                break;
            }
            case Bin: {
                for(int i = CHAR_BIT - 1; i >= 0; --i){
                    target.push_back(s_bin[(ch >> i) & 1]);
                    current_len++;
                    check_split();
                }
                break;
            }
        }
        /// @note 实现Base32/Base64的时候注意这里要按照要求加上去
        cursor++;
        processed_bytes++;
    }
    if(parent_ml)*parent_ml = current_len;
}

bool log_rate::Info::check(){
    switch(type){
    case Rate:{
        if(times_per_second > 0){
            double all = clk.get_all();
            double interval = 1000.0 / times_per_second;
            double last = last_all.load(std::memory_order::relaxed);

            if(all-last >= interval){
                if(last_all.compare_exchange_strong(last, all, std::memory_order_relaxed)){
                    return true;
                }
            }
            return false;
        }else return true;
    }
    case Once:
        if(printed.load(std::memory_order::relaxed)){
            return false;
        }
        {
            bool expected = false;
            if(printed.compare_exchange_strong(expected,true,std::memory_order::release)){
                return true;
            }
        }
        return false;
    }
    return true;
}

Logger& alib5::__get_internal_logger(){
    static Logger logger (
        []{
            LoggerConfig cfg;
            cfg.consumer_count = 0;
            return cfg;
        }()
    );
    static bool init_once = []{
        logger.append_mod<lot::Console>("console");
        return true;
    }();
    return logger;
}

void Logger::setup_consumer_threads(){
    // 创建runner
    for(size_t i = 0;i < config.consumer_count;++i){
        consumers.emplace_back(&Logger::consumer_func,this);
    }
}

void Logger::consumer_func(){
    // 靠fetch message来填充
    // 由于这个不是thread_local的，因此能保证资源被正确析构，因此不需要clear
    std::vector<LogMsg> messages;

    while(true){
        msg_semaphore.acquire();
        if(!logger_not_on_destroying){
            messages.clear();
            return;
        }
        size_t count = fetch_messages(messages);
        // 没取出一个，说明可能虚假唤醒了
        if(!count)continue;
        // 输出数据
        write_messages(std::span(messages).subspan(0,count));
    }
}

void Logger::write_messages(std::span<LogMsg> msgs,bool autoflush){
    if(!filters.empty()){
        for(auto & msg : msgs){
            if(!msg.m_nice_one)continue;
            for(auto & filter : filters){
                if(!filter->enabled)continue;
                msg.m_nice_one = filter->filter(msg);
                if(!msg.m_nice_one)break;
            }
        }
    }
    for(size_t i = 0;i < msgs.size();++i){
        auto& t = msgs[i];
        if(!t.m_nice_one)continue;
        
        t.build_on_consumer();
        for(auto& target : targets){
            if(!target->enabled)continue;
            target->write(t);
        }
    }
    if(autoflush){
        flush_targets();
    }
}

size_t Logger::fetch_messages(std::vector<LogMsg> & target){
    static LogMsgConfig default_fetch_cfg;
    // 一次性拿完
    size_t i = 0;
    size_t fetch_size;
    // 对target进行扩容
    target.resize(config.fetch_message_count_max);
    {
        std::lock_guard<std::mutex> lock(msg_lock);

        size_t cur_size = messages.size();
        if(!cur_size){
            return 0;
        }
        fetch_size = config.fetch_message_count_max<cur_size?
                                config.fetch_message_count_max:cur_size;
        for(;i < fetch_size;++i){
            /// @NOTE 加message请往后面加
            auto & msg = messages.front();
            // 调用move构造从而降低调用
            target[i] = std::move(msg);
            messages.pop_front();
        }
        message_size.fetch_sub(fetch_size,std::memory_order::relaxed);
    }
    return fetch_size;
}

Logger::Logger(const LoggerConfig & cfg)
:msg_buf()
,msg_alloc(&msg_buf)
,messages(msg_alloc)
,msg_str_buf()
,msg_str_alloc(&msg_str_buf)
,msg_semaphore(0)
,config(cfg)
,tag_buf()
,tag_alloc(&tag_buf)
,header_pool(){
    logger_not_on_destroying = true;
    back_pressure_threshold = cfg.back_pressure_multiply * 
                cfg.fetch_message_count_max * cfg.consumer_count;
    clock_gettime(time_clock_source,&start_time);
    setup_consumer_threads();
}

Logger::~Logger(){
    logger_not_on_destroying = false;
    msg_semaphore.release(consumers.size() * 10);
    flush();
}

void Logger::flush(){
    std::vector<LogMsg> msgs;
    while(true){
        size_t count = fetch_messages(msgs);
        // 似乎出错了啥的
        if(!count)break;
        write_messages(std::span(msgs).subspan(0,count),false);
    }
    msgs.clear();
    flush_targets();
}

bool Logger::push_message_pmr(int level,std::string_view head,std::pmr::string & body,const LogMsgConfig & cfg,std::pmr::vector<LogCustomTag> * tags){
    // pre filter
    for(auto & filter : filters){
        if(!filter->enabled)continue;
        if(!filter->pre_filter(level,body,cfg))return false;
    }
    LogMsg msg(msg_alloc,tag_alloc,cfg);
    size_t msg_sz = 0;

    msg.header = head;
    msg.level = level;
    msg.body = std::move(body);
    if(tags)msg.tags = std::move(*tags);
    msg.build_on_producer(start_time);
   
    if(config.consumer_count){ // 异步模式
        {
            std::lock_guard<std::mutex> lock(msg_lock);
            messages.emplace_back(std::move(msg));
            msg_sz = message_size.fetch_add(1,std::memory_order::relaxed) + 1;
        }
        msg_semaphore.release();

        bool should_digest = (config.enable_back_pressure && (msg_sz >= back_pressure_threshold));
        // 没有线程就别吃了
        if(should_digest){
            static thread_local std::vector<LogMsg> msgs;
            size_t count = fetch_messages(msgs);
            // 没取出一个，说明可能算错了啥的，基本不会发生
            if(count){
                write_messages(std::span(msgs).subspan(0,count));
                msgs.clear();
            }
        }
    }else{ // 同步模式
        // 不自动刷新从而节省性能
        write_messages(std::span(&msg,1),false);
    }
    return true;
}

std::string_view Logger::register_header(std::string_view val){
    if(val.compare("")){
        std::lock_guard<std::mutex> lock(monotic_pool_lock);
        auto it = header_pool.find(val);
        if(it != header_pool.end())return *it;
        return *header_pool.emplace(val).first;
    }else return "";
}