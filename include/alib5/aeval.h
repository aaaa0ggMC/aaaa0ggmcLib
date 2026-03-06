/**
 * @file aecs.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 尝试做简单的数学解释器
 * @version 5.0
 * @date 2026/03/06
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/03/05 
 */
#ifndef ALIB5_AEVAL
#define ALIB5_AEVAL
#include <alib5/autil.h>
#include <alib5/aparser.h>

namespace alib5::eval{
    template<class Value>
    struct ALIB5_API Operand{
        enum class Type{
            TProgress = 0,
            TOperator,
            TValue
        };
        Type type;
        Value value;
        int64_t operater_id;

        void set_op(int64_t op){ type = Type::TOperator;operater_id = op;}
        int64_t get_op() const { return operater_id; }
        void set_type(Type tp){type = tp;}
        Type get_type() const {return type;}
        void set_value(const Value & v){type = Type::TValue; value = v;}
        void set_value(Value && v){ type = Type::TValue; value = std::move(v); }
        const Value & get_value() const { return value; }

        Operand()
        :type(Type::TProgress)
        ,value()
        ,operater_id(0){}
        Operand(Operand &&) = default;
    };

    template<class ValueType>
    struct ALIB5_API Expression{
        // 这个是存在的子图
        std::pmr::vector<Expression> subs;
        // 这是RPN式子
        std::pmr::vector<Operand<ValueType>> operands;
        
        void from(std::string_view data);
    };

}

namespace alib5::eval{
    template<class ValueType>
    void Expression<ValueType>::from(std::string_view data){
        using OP = Operand<ValueType>;
        using TP = typename OP::Type;
        operands.clear();
        subs.clear();
        auto is_space = [](char ch){return std::isspace(ch);};
        auto is_char = [](char ch){
            return std::isalnum(ch) || ch == '.' || ch == '_';
        };
        auto is_number = [](char ch){
            return '0' <= ch && ch <= '9' || ch == '.';
        };
        auto to_number = [](std::string_view str) -> ValueType{
            return panalyser_t::Value(str).as<double>();
        };

        std::vector<std::string_view> op_eager = {
            "*","/","+","-",
        };
        std::vector<std::string_view> op_satsify = {
            "add","sub","mul","div"
        };
        size_t cut = 0;
        TP type;

        while(!data.empty()){
            Operand<ValueType> op;
            cut = 0;
            {
                size_t beg = 0;
                while(beg < data.size() && is_space(data[beg])){
                    ++beg;
                }
                data = data.substr(beg);
                [[unlikely]] if(data.empty())break;
            }
            // 进行贪婪的operand匹配
            for(size_t i = 0;i < op_eager.size();++i){
                if(data.starts_with(op_eager[i])){
                    cut = op_eager[i].size();
                    op.set_op(i);
                    break;
                }
            }
            // 进行不贪婪的operand匹配
            if(!cut){
                for(size_t i = 0;i < op_satsify.size();++i){
                    if(data.starts_with(op_satsify[i])){
                        if(data.size() == op_satsify[i].size() || is_space(data[op_satsify[i].size()])){
                            cut = op_satsify[i].size();
                            op.set_op(-1 * (int64_t)(i));
                            break;
                        }
                    }
                }
            }

            // 也许是一个变量/函数
            if(!cut){
                while(is_char(data[cut])){
                    ++cut;
                    if(cut >= data.size())break;
                }
                if(cut && is_number(data[0])){
                    op.set_value(to_number(data.substr(0,cut)));
                }
                else op.set_type(TP::TProgress); // 后面会扩展函数检测等等
            }

            if(cut){
                data = data.substr(cut);
            }else{
                // 报错
                std::cout << "Invalid method" << std::endl;
                break;
            }
            operands.emplace_back(std::move(op));
        }
    }
}

#endif