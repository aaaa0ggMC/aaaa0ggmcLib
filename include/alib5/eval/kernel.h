#ifndef ALIB5_AEVAL_KERNEL
#define ALIB5_AEVAL_KERNEL
#include <alib5/eval/trie.h>
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
            // 一般都返回true
            using call_t = std::function<bool(ValueType & result,std::span<ValueType> values)>;

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
        std::pmr::vector<std::array<std::basic_string_view<CharT>,2>> delims;
        CharT arg_splitter;

        std::pmr::unordered_set<std::pmr::basic_string<CharT>> buffer;
        size_t variable_current { 0 };
        std::pmr::unordered_map<std::basic_string_view<CharT>,size_t> variables;

        size_t constance_current { 0 };
        std::pmr::unordered_map<std::basic_string_view<CharT>,size_t> constances;
        std::pmr::unordered_map<std::basic_string_view<CharT>,ValueType> constance_values;

        bool check_delim(size_t check_pos,std::basic_string_view<CharT> c){
            for(auto & v : delims){
                if(v[check_pos] == c){
                    return true;
                }
            }
            return false;
        }

        bool beg_delim(size_t check_pos,std::basic_string_view<CharT> c){
            for(auto & v : delims){
                if(c.starts_with(v[check_pos])){
                    return true;
                }
            }
            return false;
        }

        std::basic_string_view<CharT> get_str(std::basic_string_view<CharT> src){
            // 暂时没搞transparent
            std::pmr::basic_string<CharT> finder(get_allocator());
            finder = src;

            auto it = buffer.find(src);
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

        bool is_left_delim(std::basic_string_view<CharT> c){
            return check_delim(0, c);
        }

        bool is_right_delim(std::basic_string_view<CharT> c){
            return check_delim(1, c);
        }

        bool beg_left_delim(std::basic_string_view<CharT> c){
            return beg_delim(0, c);
        }

        bool beg_right_delim(std::basic_string_view<CharT> c){
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
            std::cout << std::format("MP2:{}",id_span) << std::endl;
            if(!id_span.empty()){
                calls.reserve(calls.size() + id_span.size());
                for(const auto& id : id_span){
                    std::cout << "Mapping:" << id << std::endl;
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
        struct ALIB5_API Operand{
            enum class Type : int {
                TNone,
                TValue,
                TOperator,
                TVariable,
                TConstance,
                TGraph
            };

            Type type { Type::TNone };
            ValueType value { ValueType() };
            // 这个是对于operator & variable & constant & graph 一起用的
            size_t id { 0 };
        };
        rule_t & rule;
        size_t depth {0};
        // 为了防止引用失效,所有的都是deque
        // 因此expression性能会差一点
        std::pmr::deque<Expression> sub_graphs;
        std::pmr::deque<Operand> operands;


        bool from(string_t astr){
            // 清理数据
            sub_graphs.clear();
            operands.clear();
            if(astr.empty())return true;
            
            struct Frame{
                string_t str;
                /// 0 此时是空列表,因此可以接受 (数值) (函数:Deco为Left)
                /// 1 此时接受了一个数值 支持 (符号,Deco为Middle或者Right)
                /// 0(2) 此时缓冲为 数值 符号 (支持符号,Deco为Left,数值,和0等价,因此不做区分)
                size_t state;
                Expression * expression;
            };
            // 遍历生成的对象
            std::deque<Frame> frames;
            std::pmr::vector<const typename rule_t::Calls*> calls(rule.get_allocator());
            std::pmr::vector<Operand> cache_operands(rule.get_allocator());
            size_t last_operator_operand = size_t_max;

            struct Token{
                std::basic_string_view<CharT> str;
                Operand::Type type { Operand::Type::TNone };
                size_t id { 0 };
                std::pmr::vector<const typename rule_t::Calls*> calls;
                ValueType value {ValueType()};
                // 1 left delim
                int delim {0};
                std::pmr::vector<std::basic_string_view<CharT>> delim_content;
                

                Token(std::pmr::memory_resource * __a)
                :calls(__a)
                ,delim_content(__a){}
            };
            std::pmr::vector<Token> tokens;

            frames.emplace_back(astr,0,this);

            auto tokenize = [&](Frame & ff){
                auto str = ff.str;
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

                    calls.clear();
                    size_t cut = 0;
                    {
                        while(!rule.is_space(str[cut]) && 
                             !rule.is_left_delim(std::basic_string_view<CharT>(str.begin() + cut,1))
                        ){
                            ++cut;
                            if(cut >= str.size())break;
                        }
                        cut = rule.fetch_calls(str.substr(0,cut),calls);
                    }

                    // 先看看是不是delim
                    Token t(rule.get_allocator());
                    if(rule.beg_left_delim(str)){

                    }

                    if(calls.empty()){
                        cut = 0;
                        // 不存在token,得到数值
                        if(rule.is_number_begin(str[0])){
                            // 读取完整的一个数
                            // 这里多了一次冗余判断,但是为了好看,暂时不改
                            t.type = Operand::Type::TValue;
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
                                t.type = Operand::Type::TVariable;
                                t.id = index;
                            }else if(auto index = rule.is_constance(slice)){
                                t.type = Operand::Type::TConstance;
                                t.id = index;
                            }
                            t.str = slice;
                        }
                    }else{
                        t.calls = std::move(calls);
                        t.str = str.substr(0,cut);
                        t.type = Operand::Type::TOperator;
                    }
                    tokens.emplace_back(std::move(t));

                    str = str.substr(cut);
                    std::cout << "REMAINING \"" << str << "\"" << std::endl;
                }
                return true;
            };

            auto analyse = [&](Frame & f){

            };

            bool fail = false;
            while(!frames.empty()){
                Frame f = frames.back();
                frames.pop_back();

                tokens.clear();
                if(!tokenize(f))break;
                
                for(auto & t : tokens){
                    std::cout << t.str << " " << (int)t.type << " " << t.id << " " << t.value << std::endl;
                }
                //if(!analyse(f))break;
            }
            return !fail;
        }

        Expression(rule_t & irule)
        :rule(irule)
        ,sub_graphs(irule.get_allocator())
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


#endif