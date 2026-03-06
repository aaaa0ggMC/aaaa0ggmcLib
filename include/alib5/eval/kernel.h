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
            // 修饰位置
            DecoPos deco_pos { Left };
            // 这个是优先级
            size_t priority { 100 };
            // 这个是对应的调用函数
            call_t caller { nullptr };

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
    private:
        std::pmr::memory_resource * allocator;
        /// 这两个是保持等价关系的
        /// 这里平成calls主要是为了统一函数与算术符号
        /// 因此理论上就可以这么写了 +(a,b,c,d) 但是一般也没人这么写哈,一般都是 a + b 然后被检测
        std::pmr::vector<Calls> registered_calls;
        detail::TrieMapper<CharT,conf_rule_sparse> trie_mapper;

        std::function<bool(CharT)> m_is_space;
        std::function<bool(CharT)> m_is_number;
        std::function<bool(CharT)> m_is_token_chars;
        std::function<ValueType(std::basic_string_view<CharT>)> m_to_value;
        std::pmr::vector<CharT[2]> delims;
        CharT arg_splitter;

        bool check_delim(size_t check_pos,CharT c){
            for(auto & v : delims){
                if(v[check_pos] == c){
                    return true;
                }
            }
            return false;
        }

    public:
        Rule(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :allocator(__a)
        ,delims(__a)
        ,registered_calls(__a)
        ,trie_mapper(__a){
            /// @todo 这里要支持trie_mapper的字符集设置,因此Rule一开始就需要接受支持的字符集
            /// @todo 我们也可以提供默认的字符集 
        }

        bool is_space(CharT ch){
            if(m_is_space)return m_is_space(ch);
            return false;
        }

        bool is_number(CharT ch){
            if(m_is_number)return m_is_number(ch);
            return false;
        }

        bool is_left_delim(CharT c){
            return check_delim(0, c);
        }

        bool is_right_delim(CharT c){
            return check_delim(1, c);
        }

        ValueType to_value(std::basic_string_view<CharT> s){
            if(m_to_value)return m_to_value(s);
            // 所以valuetype需要提供空参版本
            return ValueType();
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
        struct ALIB5_API Operand{
            enum class Type : int {
                TNone,
                TValue,
                TOperator,
                TVariable,
                TConstant,
                TGraph
            };

            Type type { Type::TNone };
            ValueType value { ValueType() };
            // 这个是对于operator & variable & constant & graph 一起用的
            size_t id { 0 };
        };
        const rule_t & rule;
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
                Expression * expression;
            };
            // 遍历生成的对象
            std::deque<Frame> frames;
            std::pmr::vector<const typename rule_t::Calls*> calls;
            frames.emplace_back(astr,this);

            auto tokenize = [&](Frame & f){
                Operand op;
                // 跳过空白字符
                {
                    size_t cut = 0;
                    while(rule.is_space(f.str[cut])){
                        ++cut;
                        if(cut >= f.str.size())break;
                    }
                    f.str = f.str.substr(cut);
                    if(f.str.empty())return;
                }

                // 识别token
                calls.clear();
                size_t cut = 0;
                {
                    while(!rule.is_space(f.str[cut]) && !rule.is_left_delim(f.str[cut])){
                        ++cut;
                        if(cut >= f.str.size())break;
                    }
                    cut = rule.fetch_calls(f.str.substr(0,cut),calls);
                }

                if(calls.empty()){
                    cut = 0;
                    // 不存在token,得到数值
                    if(rule.is_number(f.str[0])){
                        // 读取完整的一个数
                        // 这里多了一次冗余判断,但是为了好看,暂时不改
                        while(rule.is_number(f.str[cut])){
                            ++cut;
                            if(cut >= f.str.size())break;
                        }
                        op.type = Operand::Type::TValue;
                        auto slice = f.str.substr(0,cut);
                        op.value = std::move(rule.to_value(slice));
                    }else{
                        while(rule.is_token(f.str[cut])){
                            ++cut;
                            if(cut >= f.str.size())break;
                        }
                        // 具体是variable还是constant还需要待定
                        auto slice = f.str.substr(0,cut);
                        if(rule.is_variable(slice)){
                            op.type = Operand::Type::TVariable;
                        }
                    }
                }else{
                    // 存在token,进行判断


                }
                f.str = f.str.substr(cut);
            };

            while(!frames.empty()){
                Frame f = frames.back();
                frames.pop_back();
                while(!f.str.empty()){
                    tokenize(f);
                }
            }
        }

        Expression(const rule_t & irule)
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