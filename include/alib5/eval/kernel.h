#ifndef ALIB5_AEVAL_KERNEL
#define ALIB5_AEVAL_KERNEL
#include <alib5/eval/trie.h>
#include <alib5/eval/executor.h>
#include <deque>

#ifndef ALIB5_EVAL_RULE_SPARSE
#define ALIB5_EVAL_RULE_SPARSE true
#endif

namespace alib5::eval{
    // 为了适应各种数据默认是sparse的
    constexpr bool conf_rule_sparse = ALIB5_EVAL_RULE_SPARSE;
    constexpr size_t size_t_max = std::numeric_limits<size_t>::max();
   
    /// 符号修饰位置
    enum DecoPos {
        Left,  ///< 函数一般都是这样,比如 sin xxx xxx xxx,这种deco模式下多参数依赖括号
        Right, ///< 比如"!",同样依赖括号多参数
        Middle ///< 一般的算术符号都这样,比如 a + b , a / b
    }; 

    /// Rule应该做到尽量不更改
    template<class ValueType = double ,class CharT = char >
    struct ALIB5_API Rule{
        using value_t = ValueType;
        using char_t = CharT;

        struct ALIB5_API Calls{
            /// 这个是rule自动填入的
            size_t id { 0 };
        public:
            using call_t = Caller<CharT,ValueType>;

            std::pmr::basic_string<CharT> data;
            /// 允许的最大参数个数,传入size_t_max意思不言而喻就是变参了
            size_t param_limit { size_t_max };
            // 要求最少的参数个数
            size_t param_min { 1 };
            /// 是否为左结合
            /// 比如 a ^ b ^ c,同优先级处理下因为是右结合因此解析到 a ^ b后不会停止而是压入 a ^ 移动到  b ^ c 
            bool left_based { true };
            /// 是否具有交换律,具有交换律可以优化graph
            bool swappable { true };
            /// 修饰位置
            DecoPos deco_pos { Left };
            /// 这个是优先级
            size_t priority { 100 };
            /// 这个是对应的调用函数
            call_t caller { nullptr };

            size_t get_id(){ return id; }
            
            auto& set_right_based(){ return set_left_based(false); }
            auto& set_left_based(bool left = true){ left_based = left; return *this;}
            auto& set_deco(DecoPos pos){ deco_pos = pos; return *this; }
            auto& set_priority(size_t p){ priority = p; return *this; }
            auto& set_data(std::basic_string_view<CharT> d){ data.assign(d.begin(),d.end()); return *this; }
            auto& set_param_min(size_t min_val){ param_min = min_val; return *this; }
            auto& set_param_lim(size_t limit_val){ param_limit = limit_val; return *this; }
            auto& set_unlimited_param(){ return set_param_lim(size_t_max);}
            auto& set_swappable(bool swp){ swappable = swp; return *this; }
            template<class Fn>
            auto& set_caller(Fn && fn){
                caller = std::forward<Fn>(fn);
                return *this;
            }

            Calls(std::pmr::memory_resource * __a  = ALIB5_DEFAULT_MEMORY_RESOURCE)
            :data(__a){}
        };

        struct Delim{
            std::array<std::basic_string_view<CharT>,2> delims;
            size_t mapping_id;
        };
    // private:
        std::pmr::memory_resource * allocator;
        /// 这两个是保持等价关系的
        /// 这里平成calls主要是为了统一函数与算术符号
        /// 因此理论上就可以这么写了 +(a,b,c,d) 但是一般也没人这么写哈,一般都是 a + b 然后被检测
        std::pmr::vector<Calls> registered_calls;
        detail::TrieMapper<CharT,conf_rule_sparse> trie_mapper;

        std::function<bool(CharT)> m_is_space;
        std::function<bool(CharT)> m_is_number;
        std::function<bool(CharT)> m_begin_number;
        std::function<bool(CharT)> m_is_token_chars;
        std::function<size_t(std::basic_string_view<CharT>,ValueType&)> m_to_value;
        std::pmr::vector<Delim> delims;
        std::basic_string_view<CharT> arg_splitter;

        std::pmr::unordered_set<std::pmr::basic_string<CharT>> buffer;
        size_t variable_current { 0 };
        std::pmr::unordered_map<std::basic_string_view<CharT>,size_t> variables;

        size_t constance_current { 0 };
        std::pmr::unordered_map<std::basic_string_view<CharT>,size_t> constances;
        std::pmr::unordered_map<std::basic_string_view<CharT>,ValueType> constance_values;

        size_t check_delim(size_t check_pos,std::basic_string_view<CharT> c){
            for(size_t  i = 0;i < delims.size();++i){
                if(delims[i].delims[check_pos] == c){
                    return i;
                }
            }
            return size_t_max;
        }

        auto beg_delim(size_t check_pos,std::basic_string_view<CharT> c){
            for(size_t  i = 0;i < delims.size();++i){
                if(c.starts_with(delims[i].delims[check_pos])){
                    return i;
                }
            }
            return size_t_max;
        }

        std::basic_string_view<CharT> get_str(std::basic_string_view<CharT> src){
            // 暂时没搞transparent
            std::pmr::basic_string<CharT> finder(get_allocator());
            finder = src;

            auto it = buffer.find(finder);
            if(it != buffer.end())return *it;

            return *(buffer.emplace(std::move(finder)).first);
        }

    public:
        Rule(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :allocator(__a)
        ,delims(__a)
        ,registered_calls(__a)
        ,trie_mapper(__a)
        ,buffer(__a)
        ,variables(__a)
        ,constance_values(__a)
        ,constances(__a){
            /// @todo 这里要支持trie_mapper的字符集设置,因此Rule一开始就需要接受支持的字符集
            /// @todo 我们也可以提供默认的字符集 
        }

        std::basic_string_view<CharT> get_arg_splitter(){
            return arg_splitter;
        }

        void set_arg_splitter(std::basic_string_view<CharT> s){
            arg_splitter = get_str(s);
        }

        bool add_var(std::basic_string_view<CharT> name){
            auto fd = get_str(name);
            if(variables.find(fd) != variables.end() || constances.find(fd) != constances.end()){
                return false;
            }
            variables.emplace(fd,++variable_current);
            return true;
        }

        // 支持多次修改
        bool add_const(std::basic_string_view<CharT> name,ValueType && val){
            auto fd = get_str(name);
            if(variables.find(fd) != variables.end()){
                return false;
            }
            auto it = constance_values.find(fd);
            if(it == constance_values.end()){
                constances.emplace(fd,++constance_current);
                constance_values.emplace(fd,std::move(val));
            }else{
                *it = std::move(val);
            }
            return true;
        }

        bool add_const(std::basic_string_view<CharT> name,const ValueType & val){
            ValueType t = val;
            return add_const(name,std::move(t));
        }

        // 0 表示不是
        size_t is_variable(std::basic_string_view<CharT> name){
            auto f = variables.find(name);
            return f == variables.end() ? 0 : f->second;
        }

        size_t is_constance(std::basic_string_view<CharT> name){
            auto f = constances.find(name);
            return f == constances.end() ? 0 : f->second;
        }

        Delim get_delim(size_t index){
            panic_if(index >= delims.size(),"Array out of bounds!");
            return delims[index];
        }

        const ValueType& get_constance_value(std::basic_string_view<CharT> name){
            return constance_values.find(name)->second;
        }

        bool is_space(CharT ch){
            if(m_is_space)return m_is_space(ch);
            return false;
        }

        bool is_number(CharT ch){
            if(m_is_number)return m_is_number(ch);
            return false;
        }

        bool is_number_begin(CharT ch){
            if(m_begin_number)return m_begin_number(ch);
            return false;
        }

        size_t is_left_delim(std::basic_string_view<CharT> c){
            return check_delim(0, c);
        }

        size_t is_right_delim(std::basic_string_view<CharT> c){
            return check_delim(1, c);
        }

        size_t beg_left_delim(std::basic_string_view<CharT> c){
            return beg_delim(0, c);
        }

        size_t beg_right_delim(std::basic_string_view<CharT> c){
            return beg_delim(1, c);
        }

        size_t to_value(std::basic_string_view<CharT> s,ValueType & v){
            if(m_to_value)return m_to_value(s,v);
            // 所以valuetype需要提供空参版本
            return 0;
        }

        bool is_token(CharT ch){
            if(m_is_token_chars)return m_is_token_chars(ch);
            return false;
        }

        auto add_call(const Calls & call){
            // 触发copy
            Calls c(get_allocator());
            c = call;
            return add_call(std::move(c));
        }

        auto add_delim(std::span<const std::basic_string_view<CharT>> delim,Calls && call){
            if(delim.size() < 2)return false;
            Delim d;
            d.delims[0] = get_str(delim[0]);
            d.delims[1] = get_str(delim[1]);
            d.mapping_id = registered_calls.size();
            delims.emplace_back(d);

            // 这里保证存进去的left delim
            call.data = d.delims[0];

            return add_call(std::move(call));
        }

        auto add_delim(std::span<const std::basic_string_view<CharT>> delim,const Calls & call){
            Calls c(get_allocator());
            c = call;
            return add_delim(delim,std::move(c));
        }
        
        // 应该会自己决定是move还是copy
        auto add_call(Calls && call){ 
            Calls & ref = registered_calls.emplace_back(get_allocator());
            ref = std::move(call); 
            ref.id = registered_calls.size() - 1;
            
            trie_mapper.add(ref.data,registered_calls.size() - 1);
            return true;
        }

        const Calls& get_call(size_t mapping_id) const{
            panic_if(mapping_id >= registered_calls.size(),"Array out of bounds!");
            return registered_calls[mapping_id];
        }

        size_t fetch_calls(std::basic_string_view<CharT> name,std::pmr::vector<const Calls*>& calls){
            auto [id_span, consumed] = trie_mapper.get(name); 
            if(!id_span.empty()){
                calls.reserve(calls.size() + id_span.size());
                for(const auto& id : id_span){
                    calls.emplace_back(&get_call(id));
                }
            }
            return consumed; 
        }

        std::pair<std::pmr::vector<const Calls*>,size_t> fetch_calls(std::basic_string_view<CharT> name){
            std::pmr::vector<const Calls*> calls(get_allocator());
            auto sz = fetch_calls(name,calls);
            return std::make_pair(calls,sz);
        }

        auto get_allocator(){ return allocator; }
    };

    template<class ValueType = double ,class CharT = char >
    struct ALIB5_API Expression{
        using string_t = std::basic_string_view<CharT>;
        using rule_t = Rule<ValueType,CharT>;
        
        struct Target{
            enum Type{
                None,
                Value,
                Variable,
                Constance,
                Graph
            };

            // 这个是对于operator & variable & constant & graph 一起用的
            Type type { Value };
            size_t id { 0 };
            ValueType value { ValueType() };
        };

        struct OpCode{
            /// 除了函数,绝大多数都是二元
            std::pmr::vector<Target> targets;
            const typename rule_t::Calls* b;
        };

        rule_t & rule;
        size_t depth {0};
        // 这个图的id,在from计算出的expression列表中,这个是线性的
        size_t id {0};
        std::pmr::deque<OpCode> operands;

        Expression(rule_t & irule)
        :rule(irule)
        ,operands(irule.get_allocator()){}
    };

    template<class ValueType = double ,class CharT = char >
    struct ALIB5_API Eval{
        using rule_t = Rule<ValueType,CharT>;
        const rule_t & rule;

        Eval(const rule_t & irule)
        :rule(irule){}
    };

    using rule_t = Rule<>;
    using call_t = rule_t::Calls;
    using deco_t = DecoPos;
    using expression_t = Expression<>;
    using eval_t = Eval<>;
}

namespace alib5::eval{
    template<class ValueType = double ,class CharT = char >
    inline bool from(std::basic_string_view<CharT> astr,Rule<ValueType,CharT> & rule){
        using TRule = Rule<ValueType,CharT>;
        using TCall = TRule::Calls;
        
        if(astr.empty())return true;
    
        //// 词法解析部分
        std::pmr::vector<const typename rule_t::Calls*> calls(rule.get_allocator());
        struct Token{
            enum Type{
                None,
                Value,
                Variable,
                Const,
                DelimLeft,
                DelimRight,
                ArgSplit,
                Operator
            };
            
            std::basic_string_view<CharT> str;
            Type type { None };
            size_t id { 0 };
            std::pmr::vector<const typename rule_t::Calls*> calls;
            ValueType value {ValueType()};
            const typename rule_t::Calls * delim_call;
            // 语义分析要用到的
            const typename rule_t::Calls * focus_call;
            

            Token(std::pmr::memory_resource * __a)
            :calls(__a){}
        };
        std::pmr::vector<Token> tokens;
        auto tokenize = [&](std::basic_string_view<CharT> str){
            while(!str.empty()){
                {
                    size_t cut = 0;
                    while(rule.is_space(str[cut])){
                        ++cut;
                        if(cut >= str.size())break;
                    }
                    str = str.substr(cut);
                    if(str.empty())return true;
                }
                Token t(rule.get_allocator());
                size_t cut = 0;

                // 看看是不是arg sep
                if(str.starts_with(rule.get_arg_splitter())){
                    t.type = Token::ArgSplit;
                    cut = rule.get_arg_splitter().size();
                    t.str = str.substr(0,cut);
                }
                
                // 再看看是不是delim
                if(auto d = rule.beg_left_delim(str); d != size_t_max){
                    auto delim = rule.get_delim(d);
                    auto & call = rule.get_call(delim.mapping_id);

                    t.type = Token::DelimLeft;
                    // 一定存在一个左缀call
                    rule.fetch_calls(delim.delims[0],t.calls);
                    t.delim_call = &call;
                    cut = delim.delims[0].size();
                    t.str = str.substr(0,cut);
                }else if(auto d = rule.beg_right_delim(str); d != size_t_max){
                    auto delim = rule.get_delim(d);
                    auto & call = rule.get_call(delim.mapping_id);
                    t.type = Token::DelimRight;
                    rule.fetch_calls(delim.delims[0],t.calls);
                    t.delim_call = &call;
                    cut = delim.delims.size();
                    t.str = str.substr(0,cut);
                }

                if(cut == 0){
                    calls.clear();
                    {
                        while(!rule.is_space(str[cut])){
                            ++cut;
                            if(cut >= str.size())break;
                        }
                        cut = rule.fetch_calls(str.substr(0,cut),calls);
                    }
                    if(calls.empty()){
                        cut = 0;
                        // 不存在token,得到数值
                        if(rule.is_number_begin(str[0])){
                            // 读取完整的一个数
                            // 这里多了一次冗余判断,但是为了好看,暂时不改
                            t.type = Token::Value;
                            size_t re_cut = rule.to_value(str, t.value); 
                            
                            if(re_cut == 0){
                                // 语法错误：虽然 begin_number 是 true，但无法解析成有效数值
                                return false;
                            }
                            t.str = str.substr(0, re_cut);
                            cut = re_cut;
                        }else{
                            while(rule.is_token(str[cut])){
                                ++cut;
                                if(cut >= str.size())break;
                            }
                            if(cut == 0){
                                // 这是完全不认得的东西
                                std::cout << "Unknown \"" << str << "\"!" << std::endl;
                                return false;
                            }
                            // 具体是variable还是constant还需要待定
                            auto slice = str.substr(0,cut);
                            if(auto index = rule.is_variable(slice)){
                                t.type = Token::Variable;
                                t.id = index;
                            }else if(auto index = rule.is_constance(slice)){
                                t.type = Token::Const;
                                t.id = index;
                            }
                            t.str = slice;
                        }
                    }else{
                        t.calls = std::move(calls);
                        t.str = str.substr(0,cut);
                        t.type = Token::Operator;
                    }
                }
                tokens.emplace_back(std::move(t));
                str = str.substr(cut);
            }
            return true;
        };

        if(!tokenize(astr))return false;
        auto get_str = [](Token::Type t){
            #define UU(X) case Token::X: return ""#X;
            switch(t){
                UU(ArgSplit)
                UU(None)
                UU(Value)
                UU(Variable)
                UU(Const)
                UU(DelimLeft)
                UU(DelimRight)
                UU(Operator)
            }
        };

        std::cout << std::left 
        << std::setw(12) << "Type"
        << std::setw(12) << "String"
        << std::setw(12) << "ID"
        << std::setw(10) << "Value"
        << std::endl;
        std::cout << std::string(46, '-') << std::endl;

        for(Token & t : tokens){
            std::cout << std::left
                    << std::setw(12) << get_str(t.type)
                    << std::setw(12) << (std::string("\"") + t.str + "\"")
                    << std::setw(12) << t.id
                    << std::setw(10) << t.value
                    << std::endl; 
        }

        //// 语法解析部分

        using exp_t = Expression<ValueType,CharT>;
        struct Frame{
            std::span<Token> tokens;
            /// 0 此时是空列表,因此可以接受 (数值) (函数:Deco为Left)
            /// 1 此时接受了一个数值 支持 (符号,Deco为Middle或者Right)
            /// 0(2) 此时缓冲为 数值 符号 (支持符号,Deco为Left,数值,和0等价,因此不做区分)
            size_t state;
            exp_t * expression;
        };
        // 遍历生成的对象
        std::deque<Frame> frames;
        std::pmr::deque<exp_t> expressions (rule.get_allocator());

        frames.emplace_back(Frame{
            .tokens = tokens,
            .state = 0,
            .expression = &expressions.emplace_back(rule)
        });
        
        Token empty_token(rule.get_allocator());
        empty_token.type = Token::None;
        
        std::pmr::deque<Token *> cache_line (rule.get_allocator());
        size_t computable_count = 0;
        Token * cache_line_last = nullptr;

        auto is_value = [](Token & t){
            return t.type == Token::Value || t.type == Token::Variable || t.type == Token::Const;
        };

        while(!frames.empty()){
            Frame f = frames.back(); 
            frames.pop_back();

            auto get_token = [&f,&empty_token](size_t index)->Token&{
                if(index >= f.tokens.size())return empty_token;
                return f.tokens[index];
            };

            // 规约
            auto generate = [&]{
                std::pmr::deque<Token*> left_tokens(rule.get_allocator());
                std::pmr::deque<Token*> right_tokens(rule.get_allocator());

                std::pmr::deque<Token*> values;
                std::pmr::deque<Token*> ops;

                while(!cache_line.empty()){
                    Token * t = cache_line.back();
                    cache_line.pop_back();

                    if(is_value(*t)){
                        // 往前追溯t的左op
                        while(!cache_line.empty()){
                            Token * tx = cache_line.back();
                            
                            if(tx->focus_call->deco_pos == Left){
                                left_tokens.emplace_back(tx);
                            }else break;
                            cache_line.pop_back();
                        }
                         // 进行规约
                        std::cout << "RECOGNIZED " << t->value << " "
                        << "LEFT " << left_tokens.size() << " RIGHT " << right_tokens.size() << std::endl;

                        // 这里生成这个value的左右处理,需要比较left token和right token的优先级从而进行运算
                        // 这一块类似shunting yard
                        values.emplace_back(t);

                        left_tokens.clear();
                        right_tokens.clear();

                        if(values.size() >= 2){
                            // 只剩下middle而middle绝对是二元的
                            if(ops.empty()){
                                panic("不是这样的");
                            }
                            Token * t = ops.back();
                            ops.pop_back();

                            
                            std::cout << "TOKEN " << t->str << " BET " << values[0]->value << " " << values[1]->value << std::endl;
                        }


                    }else if(t->focus_call->deco_pos == Right){
                        right_tokens.emplace_back(t);
                    }else if(t->focus_call->deco_pos == Middle){
                        ops.emplace_back(t);
                    }
                }
            };

            for(size_t index = 0;index < f.tokens.size();++index){
                Token & t = get_token(index);
                // 这个是单纯的数据
                if(f.state == 0){
                    // 在遇到第一个数值/delim之前,全部都应该是左op(delim就是左op)
                    if(t.type == Token::Operator){
                        // 尝试寻找左op
                        const TCall * found = nullptr;
                        for(auto& c : t.calls){
                            if(c->deco_pos == Left){
                                found = c;
                                break;
                            }
                        }
                        if(!found){
                            std::cout << "Invalid grammar!" << std::endl;
                            return false;
                        }
                        t.focus_call = found;
                    }else if(is_value(t)){
                        // 遇到数值切换f.state 为  1
                        ++computable_count;
                        f.state = 1;
                    }

                    cache_line.emplace_back(&t);
                }else if(f.state == 1){
                    // 一定得是一个Middle Op,如果是delim可以进行歧义判断
                    if(is_value(t)){
                        std::cout << "Invalid grammar" << std::endl;
                        return false;
                    }
                    /// 尝试寻找中/右Op
                    {
                        const TCall * found = nullptr;
                        for(auto& c : t.calls){
                            if(c->deco_pos == Middle || c->deco_pos == Right){
                                found = c;
                                break;
                            }
                        }
                        if(!found){
                            std::cout << "Invalid grammar!" << std::endl;
                            return false;
                        }
                        t.focus_call = found;
                    }
                    // 对于超出两个computable时,说明此时可以尝试进行归约
                    // 并且存在中后缀表达式可以用于比较优先级时
                    if(computable_count >= 2 && cache_line_last && 
                        (   // 条件一,优先级严格小于
                            t.focus_call->priority < cache_line_last->focus_call->priority ||
                            // 条件二,符号一致,并且左结合
                            ((t.focus_call->id == cache_line_last->focus_call->id) && t.focus_call->left_based )
                        )
                    ){
                        // 这样才尝试规约
                        generate();
                    }
                    // 之后依旧加入这个表中
                    cache_line.emplace_back(&t);
                    // 保存最后一个符号
                    cache_line_last = &t;
                    // 中缀切换模式
                    if(t.focus_call->deco_pos == Middle){
                        f.state = 0;
                    }
                }
                if(index + 1 == tokens.size()){
                    // 最后进行一次规约
                    generate();
                }
            }
        }

        return true;
    }

    template<class CharT = char >
    auto from(const CharT * sv, auto&& rule) {
        return from(std::basic_string_view<CharT>(sv), std::forward<decltype(rule)>(rule));
    }
};


#endif