#ifndef ALIB5_AEVAL_SMATH
#define ALIB5_AEVAL_SMATH
#include <alib5/eval/kernel.h>

#define X_OPS\
    GEN_OP("+", += , + , 1);\
    GEN_OP("-", -= , - , 2);\
    GEN_OP("*", *= , * , 3);\
    GEN_OP("/", /= , / , 4);\



namespace alib5::eval::math{
    // 这里你需要自行保证value type支持常见的数学操作
    template<IsValueType ValueType = double >
    inline Context<ValueType> get_context(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE){
        Context<ValueType> ctx(allocator);
        return ctx;
    }

    template<IsValueType ValueType = double >
    inline Executor<ValueType> get_executor(Context<ValueType> & ctx){
        Executor<ValueType> exec(ctx);

        #define GEN_OP(S,ROP,OP,ID) \
        exec.emplace_operator(S,\
            [](std::span<ValueType> vals,ValueType & output){\
                if(vals.size()) [[likely]] {\
                    output = vals[0];\
                    for(ValueType & v : vals.subspan(1)){\
                        output ROP v;\
                    }\
                }\
            }\
        );

        X_OPS;

        #undef GEN_OP

        // 进行一些额外设置
        (*ctx.get_op_data("+"))
            ->swappable(SwapStrategy::Full)
            .appendable();
        (*ctx.get_op_data("-"))
            ->swappable(SwapStrategy::ExceptFirst)
            .appendable(false);
        (*ctx.get_op_data("*"))
            ->swappable(SwapStrategy::Full)
            .appendable();
        (*ctx.get_op_data("/"))
            ->swappable(SwapStrategy::ExceptFirst)
            .appendable(false);
        return exec;
    }

    /// 这个是两个symbol产生expression
    #define GEN_OP(S,ROP,OP,ID)\
    template<class ValueType>\
    Expression<ValueType> operator OP (const Symbol<ValueType> & a,const Symbol<ValueType> & b){\
        [[unlikely]] panic_if(a.ctx != b.ctx,"Context not match!");\
        return GenExpression(*a.ctx, {a} , {b} , ID );\
    }

    X_OPS;

    #undef GEN_OP

    /// 这个是expression和symbol直接接入数值到expression后面
    #define GEN_OP(S,ROP,OP,ID) \
    template<class ValueType>\
    Expression<ValueType> operator OP (Expression<ValueType> a,const Symbol<ValueType> & b){\
        [[unlikely]] panic_if(&(a.ctx) != b.ctx,"Context not match!");\
        a.add({ b },ID);\
        return a;\
    }\
    template<class ValueType, std::convertible_to<ValueType> InValueType>\
    Expression<ValueType> operator OP (Expression<ValueType> a,const InValueType & b){\
        a.add({ (ValueType)b },ID);\
        return a;\
    }

    X_OPS;

    #undef GEN_OP

    /// 反过来也是直接操作?
    #define GEN_OP(S,ROP,OP,ID) \
    template<class ValueType>\
    Expression<ValueType> operator OP (const Symbol<ValueType> & b,Expression<ValueType> a){\
        [[unlikely]] panic_if(&(a.ctx) != b.ctx,"Context not match!");\
        a.add({ b },ID,false);\
        return a;\
    }\
    template<class ValueType, std::convertible_to<ValueType> InValueType>\
    Expression<ValueType> operator OP (const InValueType & b,Expression<ValueType> a){\
        a.add({ (ValueType)b },ID,false);\
        return a;\
    }

    X_OPS;

    #undef GEN_OP

    /// 接下来便是比较复杂的Expression之间的合并了,因为两个实际上用到的是同一个cacheline,因此似乎无法避免递归修改另一个对象的cacheline?
    #define GEN_OP(S,ROP,OP,ID)\
    template<class ValueType>\
    Expression<ValueType> operator OP (Expression<ValueType> lhs,Expression<ValueType> rhs){\
        [[unlikely]] panic_if(&(lhs.ctx) != &(rhs.ctx),"Context not match!");\
        lhs.merge(rhs,ID);\
        return lhs;\
    }\
    template<class ValueType>\
    Expression<ValueType> operator OP (Expression<ValueType> lhs,Expression<ValueType> && rhs){\
        [[unlikely]] panic_if(&(lhs.ctx) != &(rhs.ctx),"Context not match!");\
        lhs.merge(rhs,ID);\
        return lhs;\
    }

    X_OPS;

    #undef GEN_OP
};

#undef X_OPS

#endif