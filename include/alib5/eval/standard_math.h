#ifndef ALIB5_AEVAL_SMATH
#define ALIB5_AEVAL_SMATH
#include <alib5/eval/kernel.h>

namespace alib5::eval::math{
    // 和下面的是对齐的
    constexpr size_t conf_plus_index = 1;
    constexpr size_t conf_minus_index = 2;
    constexpr size_t conf_multiply_index = 3;
    constexpr size_t conf_divide_index = 4;

    // 这里你需要自行保证value type支持常见的数学操作
    template<IsValueType ValueType = double >
    inline Context<ValueType> get_context(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE){
        Context<ValueType> ctx(allocator);
        return ctx;
    }

    template<IsValueType ValueType = double >
    inline Executor<ValueType> get_executor(Context<ValueType> & ctx){
        Executor<ValueType> exec(ctx);

        #define GEN_OP(S,OP) \
        exec.emplace_operator(S,\
            [](std::span<ValueType> vals,ValueType & output){\
                if(vals.size()) [[likely]] {\
                    output = vals[0];\
                    for(ValueType & v : vals.subspan(1)){\
                        output OP v;\
                    }\
                }\
            }\
        );

        GEN_OP("+", += ); // 1
        GEN_OP("-", -= ); // 2
        GEN_OP("*", *= ); // 3
        GEN_OP("/", /= ); // 4

        #undef GEN_OP
        return exec;
    }


    /// 这个是两个symbol产生expression
    #define GEN_OP(OP,ID)\
    template<class ValueType>\
    Expression<ValueType> operator OP (const Symbol<ValueType> & a,const Symbol<ValueType> & b){\
        [[unlikely]] panic_if(&(a.ctx) != &(b.ctx),"Context not match!");\
        return GenExpression(a.ctx, {a} , {b} , ID );\
    }

    GEN_OP(+, conf_plus_index);
    GEN_OP(-, conf_minus_index);
    GEN_OP(*, conf_multiply_index);
    GEN_OP(/, conf_divide_index);

    #undef GEN_OP

    /// 这个是expression和symbol直接接入数值到expression后面
    #define GEN_OP(OP,ID) \
    template<class ValueType>\
    Expression<ValueType>& operator OP (Expression<ValueType> & a,const Symbol<ValueType> & b){\
        [[unlikely]] panic_if(&(a.ctx) != &(b.ctx),"Context not match!");\
        a.add({ b },ID);\
        return a;\
    }\
    template<class ValueType>\
    Expression<ValueType>&& operator OP (Expression<ValueType> && a,const Symbol<ValueType> & b){\
        [[unlikely]] panic_if(&(a.ctx) != &(b.ctx),"Context not match!");\
        a.add({ b },ID);\
        return std::move(a);\
    }

    GEN_OP(+, conf_plus_index);
    GEN_OP(-, conf_minus_index);
    GEN_OP(*, conf_multiply_index);
    GEN_OP(/, conf_divide_index);

    #undef GEN_OP

    /// 反过来也是直接操作?
    #define GEN_OP(OP,ID) \
    template<class ValueType>\
    Expression<ValueType>& operator OP (const Symbol<ValueType> & b,Expression<ValueType> & a){\
        [[unlikely]] panic_if(&(a.ctx) != &(b.ctx),"Context not match!");\
        a.add({ b },ID,false);\
        return a;\
    }\
    template<class ValueType>\
    Expression<ValueType>&& operator OP (const Symbol<ValueType> & b,Expression<ValueType> && a){\
        [[unlikely]] panic_if(&(a.ctx) != &(b.ctx),"Context not match!");\
        a.add({ b },ID,false);\
        return std::move(a);\
    }

    GEN_OP(+, conf_plus_index);
    GEN_OP(-, conf_minus_index);
    GEN_OP(*, conf_multiply_index);
    GEN_OP(/, conf_divide_index);

    #undef GEN_OP

    /// 接下来便是比较复杂的Expression之间的合并了,因为两个实际上用到的是同一个cacheline,因此似乎无法避免递归修改另一个对象的cacheline?
    /// @todo 先不做这个
};
#endif