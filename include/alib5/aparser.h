/**@file aparser.h
* @brief 简单的命令行解释器，由于之前的语法比较诡异
* @author aaaa0ggmc
* @date 2026/01/30
* @version 5.0
* @copyright Copyright(c) 2026 
*/
#ifndef ALIB5_APARSER
#define ALIB5_APARSER
#include <memory_resource>
#include <alib5/autil.h>
#include <string_view>
#include <alib5/adebug.h>
#include <cstring>

namespace alib5{
    /// 解释器结果
    struct ALIB5_API Parser{
        /// 内存池子，注意放到最前面
        std::pmr::memory_resource * resource;
        // 由于涉及到转移语序，这里不得不持有新的数据
        /// 命令头
        std::pmr::string head;
        /// 命令的后半部分，这里干脆也使用string，虽然理论上可以string_view
        std::pmr::string arg_full;
        /// 参数
        std::pmr::vector<std::pmr::string> args;

        /// 初始化数据
        ALIB5_API Parser(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE);

        /// 对一个完整的命令进行解析
        void ALIB5_API parse(std::string_view cmd,bool slice_args = true);
        /// 可以复用为命令行解释器
        void ALIB5_API from_args(int argc,const char * argv[]) noexcept;

        /// 对头进行匹配
        inline bool match(std::string_view chead){return chead == head;}
        /// 对头进行一系列匹配
        inline bool match(std::span<const std::string_view> cheads){
            for(auto h : cheads){
                if(match(h))return true;
            }
            return false;
        }
        inline bool match(std::initializer_list<std::string_view> data){
            return match(std::span(data.begin(),data.end()));
        }

        /// 分析器，提供渐进式分析
        struct ALIB5_API Analyser{
            /// 绑定的原始数据
            Parser & parser;
            /// 当前保存的数据列
            std::pmr::vector<std::string_view> inputs;
        private:
            friend class Parser;

            /// 由cursor生成sub analyser提供层进分析
            Analyser(Parser & p,std::span<std::string_view> i):
            parser(p),
            inputs(i.begin(),i.end(),parser.resource){}
        public:
            /// 数值，提供简单的分析以及cast，虽然和未来计划的aconf中的value功能重合了
            /// 但是这个命令行解释器本来也不是配置文件，不需要提供修改功能，因此单独出来
            struct Value{
                /// 数据
                std::string_view data;
                /// 构造数据
                template<IsStringLike T> Value(T&& s):data(std::forward<T>(s)){}
                /// 隐式转换              
                operator std::string_view() const{
                    return data;
                }

                /// 转换，遇到问题会invoke_error
                template<class T> auto as(){
                    return ext::to_T<T>(data);
                }

                /// 转换，遇到问题选择默认值
                template<class T> auto value_or(T && default_val){
                    if constexpr(std::is_arithmetic_v<T>){
                        std::from_chars_result r;
                        auto v = ext::to_T<T>(data,&r);
                        if(r.ec != std::errc() || (r.ptr) != data.data() + data.size()){
                            return default_val;
                        }
                        return v;
                    }else{
                        return ext::to_T<T>(data);
                    }
                }

                /// 判断转换是否顺利
                template<class T> bool expect(){
                    if constexpr(std::is_arithmetic_v<T>){
                        std::from_chars_result r;
                        ext::to_T<T>(data,&r);
                        if(r.ec != std::errc() || (r.ptr) != data.data() + data.size()){
                            return false;
                        }
                    }
                    return true;
                }

                /// @todo 留一个slot给validate
                template<class Validator> bool validate(Validator && v){
                    return v.validate(data);
                }
            };

            /// 注意！Cursor是一种事务，因此十分不建议复制成两份cursor然后执行apply
            /// 这样会造成意想不到的后果！
            struct Cursor{
                /// 分析器
                Analyser & m_analyser;
                /// 自己持有的数据
                std::span<std::string_view> data;
                /// 从m_analyser截取时的索引
                size_t parent_index { 0 };
                /// 是否是有效的cursor
                bool matched = false;

                /// 目前游标位置，只能往前走
                size_t cursor { 1 };

                // 关于prefix
                /// prefix内容（影响next中的提取）
                std::string_view prefix { "" };
                /// optional prefix中的内容
                std::string_view opt_str { "" };
                /// prefix 是否以及被处理
                bool processed {false};
                /// 是否已经commit
                bool applied {false};

                /// 这个是缓存数据
                std::pmr::vector<std::string_view> cached_data;

                /// 构造
                Cursor(Analyser & ana,std::span<std::string_view> d,size_t pi,bool mt):
                m_analyser{ana},
                data(d),
                parent_index(pi),
                matched(mt),
                cached_data(m_analyser.parser.resource){}

                /// 判断当前cursor是否有效
                operator bool() const noexcept {
                    return matched;    
                }
                
                /// 组合cursor提供 || 语法
                Cursor operator ||(const Cursor & c) noexcept {
                    panic_debug((intptr_t)&(c.m_analyser) != (intptr_t)&(m_analyser),"Conflict analyser source!");
                    if(c.matched && !matched){
                        return c;
                    }
                    return *this;
                }

                /// 获取当前的“头”
                inline Value head() noexcept {return data.empty()? "" : data[0];}
                /// 终止事务
                inline void abort(){
                    matched = false;
                }

                /// 检测下一个数据是否match
                inline bool match(std::string_view data){
                    return peek() == data;
                }
                inline bool match(std::span<const std::string_view> data){
                    for(auto c : data){
                        if(match(c))return true;
                    }
                    return false;
                }
                inline bool match(std::initializer_list<std::string_view> data){
                    return match(std::span(data.begin(),data.end()));
                }

                /// 步进下一条数据
                Value ALIB5_API next() noexcept;
                /// 尝试访问下一条数据
                Value ALIB5_API peek() const noexcept;

                /// 跳过数据
                inline void skip(size_t n = 1){
                    for(int i = 0;i < n;++i){
                        (void) next();
                    }
                }

                /// 上传数据，同时生成子analyser可用于进行进一步的访问
                Analyser ALIB5_API commit() noexcept;

                /// 这里是为了方便直接生成sub analyser而不一定需要commit
                Analyser ALIB5_API sub() noexcept;
            };

            Analyser(Parser & p):
            parser(p),
            inputs(parser.args.begin(),parser.args.end(),parser.resource){}

            /// 直接转换，方便直接进行操作
            inline Cursor as_cursor() noexcept {
                if(inputs.size() <= 1) {
                    return Cursor{*this, inputs, 0, false};
                }
                return Cursor{
                    *this,
                    std::span(inputs.begin(), inputs.end()),
                    0,
                    true
                };
            }

            /// 尝试获取当前剩余的有效数据
            inline std::span<std::string_view> peek_remains(){
                return std::span(inputs.begin()+1,inputs.end());
            }
            /// 返回剩余数据，同时清空缓存
            inline std::pmr::vector<std::string_view> remains(){
                std::pmr::vector<std::string_view> v(inputs.begin()+1,inputs.end());
                inputs.erase(inputs.begin()+1,inputs.end());
                return v;
            }
            /// 判断当前的analyser是否还有有效数据
            inline operator bool(){
                return inputs.size() > 1;
            }

            /// 匹配完整的option
            Cursor ALIB5_API with_opt(std::string_view opt) noexcept;
            /// 匹配前缀，比如 data="--port" opt="="
            /// 将会匹配 "--port=xxx" "--port = xxx" "--port xxx" "--port =xxx"
            /// 如果希望一定要 "--port="连体并且不认"--port xxx"可以设置 data="-port=" opt=""
            Cursor ALIB5_API with_prefix(std::string_view data,std::string_view opt = "") noexcept;
        
            //// 下面是一些包装函数 ////
            /// 支持多个option
            inline Cursor with_opts(std::span<const std::string_view> opts) noexcept{
                for(auto v : opts){
                    if(auto c = with_opt(v))return c;
                }
                return Cursor{
                    *this,
                    inputs,
                    0,
                    false
                };
            }
            inline Cursor with_opts(std::initializer_list<std::string_view> opts) noexcept{
                return with_opts(std::span<const std::string_view>(opts.begin(),opts.end()));
            }
            /// 支持多个prefix
            inline Cursor with_prefixes(std::span<const std::string_view> prefixes,std::string_view opt_str) noexcept{
                for(auto v : prefixes){
                    if(auto c = with_prefix(v,opt_str))return c;
                }
                return Cursor{
                    *this,
                    inputs,
                    0,
                    false
                };
            }
            inline Cursor with_prefixes(std::initializer_list<std::string_view> prefixes,std::string_view opt_str) noexcept{
                return with_prefixes(std::span(prefixes.begin(),prefixes.end()),opt_str);
            }

            //// 提供懒人级别的api ////
            /// 提取是否含有
            inline bool extract_first_flag(std::string_view flag){
                if(auto c =  with_opt(flag)){
                    c.commit();
                    return true;
                }
                return false;
            }
            /// 提取是否含有，！！！会移除所有重复的flag
            inline bool extract_flag(std::string_view flag){
                bool ret = false;
                while(extract_first_flag(flag)){
                    ret = true;
                }
                return ret;
            }
            /// 会逐步提取所有重复内容直到没有额外信息
            inline bool extract_first_any_flag(std::span<const std::string_view> flags){
                for(auto c : flags){
                    if(extract_first_flag(c))return true;
                }
                return false;
            }
            /// 会逐步提取所有重复内容直到没有额外信息
            inline bool extract_any_flag(std::span<const std::string_view> flags){
                bool val = false;
                for(auto c : flags){
                    val |= extract_flag(c);
                }
                return val;
            }
            /// 会逐步提取所有重复内容直到没有额外信息
            inline bool extract_any_flag(std::initializer_list<const std::string_view> flags){
                return extract_any_flag(std::span(flags.begin(),flags.end()));
            }

            //// 对于 XXX = 这种具有赋值意味的东西
            inline Value extract_an_option(std::string_view key,std::string_view opt = "="){
                if(auto c = with_prefix(key)){
                    Value v = c.next();
                    c.commit();
                    return v;
                }
                return "";
            }
            inline std::pmr::vector<Value> extract_options(std::string_view key,std::string_view opt = "="){
                std::pmr::vector<Value> ret (parser.resource);
                while(true){
                    if(auto c = with_prefix(key,opt)){
                        ret.emplace_back(c.next());
                        c.commit();
                        continue;
                    }
                    break;
                }
                return ret;
            }

            /// 支持别名的选项提取：extract_any_option({"--port", "-p"}, "8080")
            inline Value extract_any_option(std::initializer_list<std::string_view> keys, std::string_view opt = "=") {
                for(auto key : keys) {
                    if(auto c = with_prefix(key, opt)) {
                        Value v = c.next();
                        c.commit();
                        return v;
                    }
                }
                return "";
            }
        };

        /// 返回对应的分析器
        inline Analyser analyse(){
            return Analyser(*this);
        }

        /// 带有管道符号的解析
        std::pmr::vector<Analyser> ALIB5_API analyse_pipe();

        /// 简单化分析器具
        inline Analyser operator()(){
            return Analyser(*this);
        }
    };

    using pparset_t = Parser;
    using panalyser_t = Parser::Analyser;
    using pcursor_t = Parser::Analyser::Cursor;
    using pvalue_t = Parser::Analyser::Value;
};

#endif