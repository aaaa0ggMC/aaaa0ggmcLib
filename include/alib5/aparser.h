/**
 * @file aparser.h
 * @brief Command-line tokenizer and progressive analyser with transactional cursors. / 命令行词法解析器，提供渐进式分析与事务性游标。
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 *
 * @par Original Comment:
 * 简单的命令行词法解析器，上一个版本语法比较诡异
 */
#ifndef ALIB5_APARSER
#define ALIB5_APARSER
#include <memory_resource>
#include <alib5/autil.h>
#include <string_view>
#include <alib5/adebug.h>
#include <cstring>

namespace alib5{
    /// @brief Parsed command result holding head, full argument span, and per-token args. 解释器结果
    struct ALIB5_API Parser{
        /// @brief PMR memory pool; declared first so other fields can reference it. 内存池子，注意放到最前面
        std::pmr::memory_resource * resource;
        /// @brief Command head token; held as owning string because reordering transfers may occur. 命令头
        ///
        /// @par Original Comment:
        /// 由于涉及到转移语序，这里不得不持有新的数据
        std::pmr::string head;
        /// @brief Tail portion of the command after the head; stored as owning string for simplicity. 命令的后半部分，这里干脆也使用string，虽然理论上可以string_view
        std::pmr::string arg_full;
        /// @brief Split argument tokens. 参数
        std::pmr::vector<std::pmr::string> args;

        /// @brief Construct parser bound to the given memory resource. 初始化数据
        ALIB5_API Parser(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE);

        /// @brief Tokenize a full command line; optionally split the tail into args. 对一个完整的命令进行解析
        void ALIB5_API parse(std::string_view cmd,bool slice_args = true);
        /// @brief Populate from argv/argc (e.g. for use as a CLI interpreter). 可以复用为命令行解释器
        void ALIB5_API from_args(int argc,const char * argv[]) ALIB5_NOEXCEPT;

        /// @brief Return true if the head equals @p chead. 对头进行匹配
        inline bool match(std::string_view chead){return chead == head;}
        /// @brief Return true if the head matches any entry in @p cheads. 对头进行一系列匹配
        inline bool match(std::span<const std::string_view> cheads){
            for(auto h : cheads){
                if(match(h))return true;
            }
            return false;
        }
        /// @brief Convenience overload accepting an initializer_list of candidate heads.
        inline bool match(std::initializer_list<std::string_view> data){
            return match(std::span(data.begin(),data.end()));
        }

        /// @brief Progressive analyser over parser inputs supporting transactional cursors. 分析器，提供渐进式分析
        struct ALIB5_API Analyser{
            /// @brief Reference to the owning parser. 绑定的原始数据
            Parser & parser;
            /// @brief Current working input slice (mutated as cursors commit). 当前保存的数据列
            std::pmr::vector<std::string_view> inputs;
        private:
            friend class Parser;

            /// @brief Internal constructor building a sub-analyser from a cursor span. 由cursor生成sub analyser提供层进分析
            Analyser(Parser & p,std::span<std::string_view> i):
            parser(p),
            inputs(i.begin(),i.end(),parser.resource){}
        public:
            /// @brief Lightweight value wrapper carrying validity and a string_view payload; supports cast and validation. 数值，提供简单的分析以及cast，虽然和未来计划的aconf中的value功能重合了
            ///
            /// @par Original Comment:
            /// 数值，提供简单的分析以及cast，虽然和未来计划的aconf中的value功能重合了
            /// 但是这个命令行解释器本来也不是配置文件，不需要提供修改功能，因此单独出来
            struct Value{
                /// @brief Whether the value carries valid data. bool
                bool valid;
                /// @brief Underlying string view payload. 数据
                std::string_view data;
                /// @brief Construct a valid value from any string-like input. 构造数据
                template<IsStringLike T> Value(T&& s):data(std::forward<T>(s)){
                    valid = true;
                }
                /// @brief Construct an invalid value (marks the payload empty). 标志invalid
                Value(bool s):data(""){
                    valid = false;
                }
                /// @brief Truthiness test backed by @ref valid. 转化为bool
                operator bool() const{
                    return valid;
                }

                /// @brief Return the underlying string_view payload.
                std::string_view view() const {
                    return data;
                }

                /// @brief Convert to T, invoking error handler on failure. 转换，遇到问题会invoke_error
                template<class T> auto as(){
                    if(!valid){
                        invoke_error(err_format_error,"Value is invalid!");
                    }
                    return ext::to_T<T>(data);
                }

                /// @brief Convert to T returning @p default_val when invalid or unparseable. 转换，遇到问题选择默认值
                template<class T> auto value_or(T && default_val){
                    if constexpr(std::is_arithmetic_v<T>){
                        if(!valid)return default_val;
                        std::from_chars_result r;
                        auto v = ext::to_T<T>(data,&r);
                        if(r.ec != std::errc() || (r.ptr) != data.data() + data.size()){
                            return default_val;
                        }
                        return v;
                    }else{
                        if(!valid)return decltype(ext::to_T<T>(data))(default_val);
                        return ext::to_T<T>(data);
                    }
                }

                /// @brief Convert to optional<T>, returning nullopt on invalid or unparseable input. 判断转换是否顺利
                template<class T> std::optional<T> expect(){
                    if(!valid)return std::nullopt;
                    if constexpr(std::is_arithmetic_v<T>){
                        std::from_chars_result r;
                        auto v = ext::to_T<T>(data,&r);
                        if(r.ec != std::errc() || (r.ptr) != data.data() + data.size()){
                            return std::nullopt;
                        }
                        return { v };
                    }
                    return { as<T>() };
                }

                /// @todo 留一个slot给validate
                /// @brief Run @p v.validate(data); invalid values always fail. (Validate slot reserved.)
                template<class Validator> bool validate(Validator && v){
                    if(!valid)return false;
                    return v.validate(data);
                }
            };

            /// @brief Transactional cursor over a span of inputs; supports peek/next/commit/abort. 注意！Cursor是一种事务，因此十分不建议复制成两份cursor然后执行apply
            ///
            /// @par Original Comment:
            /// 注意！Cursor是一种事务，因此十分不建议复制成两份cursor然后执行apply
            /// 这样会造成意想不到的后果！
            struct Cursor{
                /// @brief Owning analyser. 分析器
                Analyser & m_analyser;
                /// @brief Span of inputs this cursor walks. 自己持有的数据
                std::span<std::string_view> data;

                /// @brief Index into the parent analyser's inputs where this cursor's span begins. 从m_analyser截取时的索引
                size_t parent_index { 0 };
                /// @brief Current cursor position; advances forward only. 目前游标位置，只能往前走
                size_t cursor { 1 };

                // 关于prefix
                /// @brief Prefix content influencing how next() extracts tokens. prefix内容（影响next中的提取）
                std::string_view prefix { "" };
                /// @brief Optional separator string following the prefix. optional prefix中的内容
                std::string_view opt_str { "" };

                /// @brief Whether this cursor is currently in a matched state. 是否是有效的cursor
                bool matched = false;
                /// @brief Whether the prefix has already been consumed. prefix 是否以及被处理
                bool processed {false};
                /// @brief Whether this cursor's changes have been committed. 是否已经commit
                bool applied {false};

                /// @brief Lazily-populated cache of consumed tokens. 这个是缓存数据
                std::optional<std::pmr::vector<std::string_view>> cached_data {std::nullopt};

                /// @brief Construct a cursor bound to @p ana over span @p d with parent index, matched flag, and start offset. 构造
                Cursor(Analyser & ana,std::span<std::string_view> d,size_t pi,bool mt,size_t beg = 1):
                m_analyser{ana},
                data(d),
                parent_index(pi),
                matched(mt),
                cursor(beg){}

                /// @brief Truthiness: cursor is usable while matched and not past the end. 判断当前cursor是否有效
                operator bool() const ALIB5_NOEXCEPT {
                    /// 理论上可以走到end(),因为subspan不包含end
                    return matched && cursor <= data.size();
                }

                /// @brief Pick the matched cursor between this and @p c (must share analyser). 组合cursor提供 || 语法
                Cursor operator ||(const Cursor & c) ALIB5_NOEXCEPT {
                    panic_debug((intptr_t)&(c.m_analyser) != (intptr_t)&(m_analyser),"Conflict analyser source!");
                    if(c.matched && !matched){
                        return c;
                    }
                    return *this;
                }

                /// @brief Return the head (first element) of the span, or empty value if span is empty. 获取当前的"头"
                inline Value head() ALIB5_NOEXCEPT {return data.empty()? "" : data[0];}
                /// @brief Mark this cursor as no longer matched. 终止事务
                inline void abort(){
                    matched = false;
                }

                /// @brief Peek the next token and compare against @p data. 检测下一个数据是否match
                inline bool match(std::string_view data){
                    auto p = peek();
                    return p && p.data == data;
                }
                /// @brief Overload matching the next token against any of @p data.
                inline bool match(std::span<const std::string_view> data){
                    for(auto c : data){
                        if(match(c))return true;
                    }
                    return false;
                }
                /// @brief Overload accepting an initializer_list.
                inline bool match(std::initializer_list<std::string_view> data){
                    return match(std::span(data.begin(),data.end()));
                }

                /// @brief True when the cursor has reached the end of its span.
                inline bool reached_end(){ return cursor >= data.size(); }

                /// @brief Return an invalid (empty) analyser. 返回一个失效的analyser
                Analyser ALIB5_API invalid() ALIB5_NOEXCEPT;

                /// @brief Advance and return the next token. 步进下一条数据
                Value ALIB5_API next() ALIB5_NOEXCEPT;
                /// @brief Advance returning a (value, optional-value) bundle when an optional separator is present. 步进下一条数据
                std::pair<Value,Value> ALIB5_API next_value_bundle(std::string_view opt) ALIB5_NOEXCEPT;
                /// @brief Return the next token without advancing. 尝试访问下一条数据
                Value ALIB5_API peek() const ALIB5_NOEXCEPT;
                /// @brief Peek a (value, optional-value) bundle for the upcoming token. 数据组
                std::pair<Value,Value> ALIB5_API peek_value_bundle(std::string_view opt) ALIB5_NOEXCEPT;

                /// @brief Skip @p n tokens. 跳过数据
                inline void skip(size_t n = 1){
                    for(int i = 0;i < n;++i){
                        (void) next();
                    }
                }

                /// @brief Commit consumed tokens to the parent analyser and return a sub-analyser for further access. 上传数据，同时生成子analyser可用于进行进一步的访问
                Analyser ALIB5_API commit() ALIB5_NOEXCEPT;

                /// @brief Build a sub-analyser without requiring a commit (non-destructive). 这里是为了方便直接生成sub analyser而不一定需要commit
                Analyser ALIB5_API sub() ALIB5_NOEXCEPT;
            };

            /// @brief Construct an analyser bound to @p p, copying its args as inputs.
            Analyser(Parser & p):
            parser(p),
            inputs(parser.args.begin(),parser.args.end(),parser.resource){}

            /// @brief Build a cursor over inputs starting at @p beg. 直接转换，方便直接进行操作
            inline Cursor as_cursor(size_t beg = 0) ALIB5_NOEXCEPT {
                if(inputs.size() <= beg + 1) {
                    return Cursor{*this, inputs, 0, false};
                }
                return Cursor{
                    *this,
                    std::span(inputs.begin() + beg, inputs.end()),
                    beg,
                    true
                };
            }

            /// @brief Return the remaining inputs past the head. 尝试获取当前剩余的有效数据
            inline std::span<std::string_view> peek_remains(){
                return std::span(inputs.begin()+1,inputs.end());
            }
            /// @brief Move remaining inputs out, clearing the cache. 返回剩余数据，同时清空缓存
            inline std::pmr::vector<std::string_view> remains(){
                std::pmr::vector<std::string_view> v(inputs.begin()+1,inputs.end());
                inputs.erase(inputs.begin()+1,inputs.end());
                return v;
            }
            /// @brief True when more than just the head remains. 判断当前的analyser是否还有有效数据
            inline operator bool(){
                return inputs.size() > 1;
            }

            /// @brief Match a complete option token (e.g. "--port"). 匹配完整的option
            Cursor ALIB5_API with_opt(std::string_view opt,size_t beg = 1) ALIB5_NOEXCEPT;
            /// @brief Match a prefix token followed by an optional separator.
            ///
            /// @par Original Comment:
            /// 匹配前缀，比如 data="--port" opt="="
            /// 将会匹配 "--port=xxx" "--port = xxx" "--port xxx" "--port =xxx"
            /// 如果希望一定要 "--port="连体并且不认"--port xxx"可以设置 data="-port=" opt=""
            Cursor ALIB5_API with_prefix(std::string_view data,std::string_view opt = "",size_t beg = 1) ALIB5_NOEXCEPT;

            //// 下面是一些包装函数 ////
            /// @brief Try multiple option tokens, returning the first match. 支持多个option
            inline Cursor with_opts(std::span<const std::string_view> opts,size_t beg = 1) ALIB5_NOEXCEPT{
                for(auto v : opts){
                    if(auto c = with_opt(v,beg))return c;
                }
                return Cursor{
                    *this,
                    inputs,
                    0,
                    false
                };
            }
            /// @brief Initializer_list overload of with_opts.
            inline Cursor with_opts(std::initializer_list<std::string_view> opts,size_t beg = 1) ALIB5_NOEXCEPT{
                return with_opts(std::span<const std::string_view>(opts.begin(),opts.end()),beg);
            }
            /// @brief Try multiple prefix tokens, returning the first match. 支持多个prefix
            inline Cursor with_prefixes(std::span<const std::string_view> prefixes,std::string_view opt_str,size_t beg = 1) ALIB5_NOEXCEPT{
                for(auto v : prefixes){
                    if(auto c = with_prefix(v,opt_str,beg))return c;
                }
                return Cursor{
                    *this,
                    inputs,
                    0,
                    false
                };
            }
            /// @brief Initializer_list overload of with_prefixes.
            inline Cursor with_prefixes(std::initializer_list<std::string_view> prefixes,std::string_view opt_str,size_t beg = 1) ALIB5_NOEXCEPT{
                return with_prefixes(std::span(prefixes.begin(),prefixes.end()),opt_str,beg);
            }

            //// 提供懒人级别的api ////
            /// @brief Return true and consume @p flag if it appears. 提取是否含有
            inline bool extract_first_flag(std::string_view flag,size_t beg = 1){
                if(auto c =  with_opt(flag,beg)){
                    c.commit();
                    return true;
                }
                return false;
            }
            /// @brief Repeatedly remove @p flag; returns true if at least one was present. 提取是否含有，！！！会移除所有重复的flag
            inline bool extract_flag(std::string_view flag,size_t beg = 1){
                bool ret = false;
                while(extract_first_flag(flag,beg)){
                    ret = true;
                }
                return ret;
            }
            /// @brief Remove the first occurrence of any flag in @p flags. 会逐步提取所有重复内容直到没有额外信息
            inline bool extract_first_any_flag(std::span<const std::string_view> flags,size_t beg  = 1){
                for(auto c : flags){
                    if(extract_first_flag(c,beg))return true;
                }
                return false;
            }
            /// @brief Remove all occurrences of every flag in @p flags. 会逐步提取所有重复内容直到没有额外信息
            inline bool extract_any_flag(std::span<const std::string_view> flags,size_t beg = 1){
                bool val = false;
                for(auto c : flags){
                    val |= extract_flag(c,beg);
                }
                return val;
            }
            /// @brief Initializer_list overload of extract_any_flag. 会逐步提取所有重复内容直到没有额外信息
            inline bool extract_any_flag(std::initializer_list<const std::string_view> flags,size_t beg = 1){
                return extract_any_flag(std::span(flags.begin(),flags.end()),beg);
            }

            //// 对于 XXX = 这种具有赋值意味的东西
            /// @brief Extract a single value associated with @p key (e.g. "--port=8080").
            inline Value extract_an_option(std::string_view key,std::string_view opt = "=",size_t beg = 1){
                if(auto c = with_prefix(key,opt,beg)){
                    Value v = c.next();
                    c.commit();
                    return v;
                }
                return "";
            }
            /// @brief Extract all values for @p key, repeating until exhausted.
            inline std::pmr::vector<Value> extract_options(std::string_view key,std::string_view opt = "=",size_t beg = 1){
                std::pmr::vector<Value> ret (parser.resource);
                while(true){
                    if(auto c = with_prefix(key,opt,beg)){
                        ret.emplace_back(c.next());
                        c.commit();
                        continue;
                    }
                    break;
                }
                return ret;
            }

            /// @brief Extract a value for the first matching key in @p keys (alias support). 支持别名的选项提取：extract_any_option({"--port", "-p"}, "8080")
            inline Value extract_any_option(std::initializer_list<std::string_view> keys, std::string_view opt = "=",size_t beg = 1) {
                for(auto key : keys) {
                    if(auto c = with_prefix(key, opt,beg)) {
                        Value v = c.next();
                        c.commit();
                        return v;
                    }
                }
                return "";
            }
        };

        /// @brief Build an analyser over this parser's inputs. 返回对应的分析器
        inline Analyser analyse(){
            return Analyser(*this);
        }

        /// @brief Split the input on pipe symbols and return one analyser per segment. 带有管道符号的解析
        std::pmr::vector<Analyser> ALIB5_API analyse_pipe();

        /// @brief Shorthand for analyse(). 简单化分析器具
        inline Analyser operator()(){
            return Analyser(*this);
        }
    };

    /// @brief Alias for Parser. Alias for Parser.
    using pparset_t = Parser;
    /// @brief Alias for Parser::Analyser.
    using panalyser_t = Parser::Analyser;
    /// @brief Alias for Parser::Analyser::Cursor.
    using pcursor_t = Parser::Analyser::Cursor;
    /// @brief Alias for Parser::Analyser::Value.
    using pvalue_t = Parser::Analyser::Value;
};

#endif
