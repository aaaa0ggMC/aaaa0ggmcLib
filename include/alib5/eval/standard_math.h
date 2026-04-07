#ifndef ALIB5_AEVAL_SMATH
#define ALIB5_AEVAL_SMATH
#include <alib5/eval/kernel.h>

namespace alib5::eval::math{
    // 和下面的是对齐的
    constexpr size_t conf_plus_index = 1;

    // 这里你需要自行保证value type支持常见的数学操作
    template<IsValueType ValueType = double >
    inline Context<ValueType> get_context(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE){
        Context<ValueType> ctx(allocator);
        return ctx;
    }

    template<IsValueType ValueType = double >
    inline Executor<ValueType> get_executor(Context<ValueType> & ctx){
        Executor<ValueType> exec(ctx);
        
        // id = 1
        exec.emplace_operator("+",
            [](std::span<ValueType> vals,ValueType & output){
                for(ValueType & v : vals){
                    output += v;
                }
            }
        );
        
        return exec;
    }


    // 一些符号重载
    template<class ValueType>
    Expression<ValueType> operator+(const Symbol<ValueType> & a,const Symbol<ValueType> & b){
        [[unlikely]] panic_if(&(a.ctx) != &(b.ctx),"Context not match!");
        
        Expression<ValueType> exp(a.ctx);
        // 往a的context里面也注入b的信息?
        // 但是值信息呢
        // 这个是完全新的内容因此直接加入就行了,不需要考虑cache
        Operation<ValueType> op(a.ctx.get_allocator());
        op.output_index = 0;
        op.targets.emplace_back(
            typename Operation<ValueType>::OpTarget{
                a
            }
        );
        op.targets.emplace_back(
            typename Operation<ValueType>::OpTarget{
                b
            }
        );
        op.op.id = conf_plus_index;

        exp.operations.emplace_back(
              std::move(op)
        );

        // 我是傻逼,忘记return了
        return exp;
    }

};
#endif