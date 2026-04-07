/**
 * @file kernel.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一个简单的解析运算库
 * @version 5.0
 * @date 2026-04-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AEVAL_KERNEL
#define ALIB5_AEVAL_KERNEL
#include <alib5/autil.h>
#include <alib5/adebug.h>

namespace alib5::eval{
    constexpr uint32_t conf_cacheline_auto_expand = 4;

    template<class T>
    concept IsValueType = requires(T & b,T & v){
        T();
        b = v;
        b = std::move(v);
    }; 

    /// symbol走这个没啥问题,不过op我觉得可能不太建议走这个?
    template<
        IsValueType ValueType = double     
    >
    struct ALIB5_API Context{
    private:
        std::pmr::memory_resource * allocator;

        alib5::str::StringPool<std::pmr::string> pool;

        // --- Symbols ---
        struct SymbolData{
            std::string_view name;
            ValueType value { ValueType() };

            SymbolData(std::string_view name,std::pmr::memory_resource * allocator)
            :name(name){}
        };
        // 记录symbol到底是什么,我就简单地使用string表示了
        // 其实symbol是连续分配的,本质上也没有啥
        std::pmr::vector<
            SymbolData
        > symbol_strings;
        // 反向查询
        std::pmr::unordered_map<
            std::string_view,
            uint64_t
        > symbol_rev_mapper;

        // --- Ops ---
        struct OpData{
            std::string_view name;

            OpData(std::string_view name,std::pmr::memory_resource * allocator)
            :name(name){}
        };
        std::pmr::vector<
            OpData
        > ops;
        // 反向查询
        std::pmr::unordered_map<
            std::string_view,
            uint64_t
        > op_rev_mapper;

    public:
        Context(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :allocator(allocator)
        ,pool(allocator)
        ,symbol_strings(allocator)
        ,symbol_rev_mapper(allocator){}

        auto get_allocator() const { return allocator; }

        // 其实这个就是保证一个唯一性
        /// @note 注意id是从1开始的但是内存中存储是从0开始的
        std::pair<
            std::string_view,
            uint64_t
        > emplace_symbol(std::string_view name){
            auto it = symbol_rev_mapper.find(name);
            if(it != symbol_rev_mapper.end())
                return *it;
            std::string_view id = std::string_view(symbol_strings.emplace_back(
                SymbolData(pool.get(name),allocator)
            ).name);
            symbol_rev_mapper.emplace(
                id,
                symbol_strings.size()
            );
            // 这个就说明从1开始了
            return {id,symbol_strings.size()};
        }

        /// @note 同emplace_symbol
        /// 这里需要强制指定op执行的是什么function
        std::pair<
            std::string_view,
            uint64_t
        > emplace_op(std::string_view name){
            auto it = op_rev_mapper.find(name);
            if(it != op_rev_mapper.end())
                return *it;
            std::string_view id = std::string_view(ops.emplace_back(
                OpData(pool.get(name),allocator)
            ).name);
            op_rev_mapper.emplace(
                id,
                ops.size()
            );
            // 这个就说明从1开始了
            return {id,ops.size()};
        }

        SymbolData& get_symbol_data(uint64_t id){
            [[unlikely]] panicf_if(
                id == 0 || id > symbol_strings.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                symbol_strings.size(),
                id
            );
            return symbol_strings[id - 1];
        }

        const SymbolData& get_symbol_data(uint64_t id) const {
            [[unlikely]] panicf_if(
                id == 0 || id > symbol_strings.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                symbol_strings.size(),
                id
            );
            return symbol_strings[id - 1];
        }

        OpData& get_op_data(uint64_t id){
            [[unlikely]] panicf_if(
                id == 0 || id > ops.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                ops.size(),
                id
            );
            return ops[id - 1];
        }

        const OpData& get_op_data(uint64_t id) const {
            [[unlikely]] panicf_if(
                id == 0 || id > ops.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                ops.size(),
                id
            );
            return ops[id - 1];
        }
    };

    // 是整个操作里的值,包括变量,常量(数值的话另说)
    template<IsValueType ValueType = double >
    struct Symbol{
        using TContext = Context<ValueType>;

        TContext & ctx;
        // --- Symbol ID ---
        uint64_t id;

        Symbol(std::string_view name,TContext & ctx)
        :ctx(ctx){
            auto i = ctx.emplace_symbol(name);
            this->id = i.second;
        }

        std::string_view get_name() const {
            return ctx.get_symbol_data(id).name;
        }

        ValueType& operator()(){
            return ctx.get_symbol_data(id).value;
        }

        const ValueType& operator()() const {
            return ctx.get_symbol_data(id).value;
        }

        template<class U>
        Symbol& operator=(U && val){
            ctx.get_symbol_data(id).value = std::forward<U>(val);
            return *this;
        }
    };
    // 便利函数,说实话这个模板函数如果我转发的话代码量还大了点
    template<IsValueType T = double ,IsStringLike... Ts>
    auto Symbols(Context<T> & ctx,Ts&&... symbols){
        return std::make_tuple(
            Symbol<T>(std::string_view(std::forward<Ts>(symbols)),ctx)...
        );
    }

    template<IsValueType ValueType = double >
    struct CacheLine{
        using TContext = Context<ValueType>;
        TContext & ctx;
        std::pmr::vector<ValueType> values;
        std::pmr::vector<ValueType> input;

        CacheLine(TContext & ctx)
        :ctx(ctx){}

        ValueType& operator[](size_t index){
            [[unlikely]] panicf_debug(
                index >= values.size() + conf_cacheline_auto_expand,
                "Index of of bounds!Required [0,{}) ,received {}.",
                values.size() + conf_cacheline_auto_expand,
                index
            );
            if(index >= values.size()) [[unlikely]] {
                values.resize(index + 1, ValueType() );
            }
            return values[index];
        }
    };

    template<IsValueType ValueType = double >
    struct Operation{
        using TSymbol = Symbol<ValueType>;
        struct Cacheline{
            uint64_t index; // 不是id,这个从0开始
        };

        struct OpTarget{
            std::variant<
                TSymbol,
                Cacheline
            > data;
        };

        struct Op{
            uint64_t id;
        };

        // 实际上可以接受一堆op_targets吧
        // 反正至少有一个,因此直接使用左侧的context不就行了?
        // 默认的计算结果都写回cacheline0号位置?
        std::pmr::vector<OpTarget> targets;
        Op op;
        uint32_t output_index { 0 }; // 写入的位置,默认都是0,显然是可以调整的

        Operation(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :targets(allocator){}
    };

    template<IsValueType ValueType = double >
    struct ALIB5_API Expression{
        using TContext = Context<ValueType>;
        using TOperation = Operation<ValueType>;
        TContext & ctx;
        std::pmr::vector<TOperation> operations;
        uint64_t children_output_index { 0 };

        uint64_t next_output_index(){
            return ++children_output_index;
        }

        void add(
            typename Operation<ValueType>::OpTarget a,
            size_t id,
            bool add_after = true
        ){
            Operation<ValueType> op(ctx.get_allocator());
            op.output_index = 0;
            if(add_after){
                op.targets.emplace_back(
                    typename Operation<ValueType>::Cacheline{ 0 }
                );
                op.targets.emplace_back(std::move(a));
            }else{
                op.targets.emplace_back(std::move(a));
                op.targets.emplace_back(
                    typename Operation<ValueType>::Cacheline{ 0 }
                );
            }
            op.op.id = id;

            operations.emplace_back(
                std::move(op)
            );
        }

        Expression(TContext & ctx)
        :ctx(ctx){}
    };

    /// 一些生成expression的辅助函数
    template<class ValueType = double >
    Expression<ValueType> GenExpression(
        Context<ValueType> & ctx,
        typename Operation<ValueType>::OpTarget a,
        typename Operation<ValueType>::OpTarget b,
        size_t id
    ){
        Expression<ValueType> exp(ctx);
        Operation<ValueType> op(ctx.get_allocator());
        op.output_index = 0;
        op.targets.emplace_back(std::move(a));
        op.targets.emplace_back(std::move(b));
        op.op.id = id;

        exp.operations.emplace_back(
            std::move(op)
        );
        return exp;
    }

    template<IsValueType ValueType = double >
    struct ALIB5_API Executor{
        using TContext = Context<ValueType>;
        using TSymbol = Symbol<ValueType>;
        using TCacheLine = CacheLine<ValueType>;
        using TOperation = Operation<ValueType>;
        using TExpression = Expression<ValueType>;
        using OpFunc = std::move_only_function<
            void(
                std::span<ValueType> input,
                ValueType & output
            )
        >;
        TContext & ctx;
    
        struct OpData{
            OpFunc func;
        };

        std::pmr::unordered_map<
            uint64_t,
            OpData
        > funcs;

        Executor(TContext & ctx)
        :ctx(ctx){}

        void emplace_operator(std::string_view name,OpFunc fn){
            OpData & data = funcs[ctx.emplace_op(name).second];
            data.func = std::move(fn);
        }

        auto execute(const TExpression & exp){
            CacheLine cache(ctx);
            return execute(exp,cache);
        }

        // 简单signal一下
        ValueType execute(const TExpression& exp,TCacheLine & cache){
            size_t latest = 0;
            // 先就是简单的执行吧
            for(const TOperation& op : exp.operations){
                auto it = funcs.find(op.op.id);
                if(it == funcs.end()){
                    // 先panic吧
                    panic("Invalid sequence.");
                }
                
                // 构建input
                cache.input.clear();
                for(const typename TOperation::OpTarget & t : op.targets){
                    std::visit([&](auto & val){
                        using T = std::decay_t<decltype(val)>;
                        if constexpr(std::is_same_v<T,TSymbol>){
                            cache.input.emplace_back(
                                ctx.get_symbol_data(val.id).value
                            );
                        }else if(std::is_same_v<T,typename TOperation::Cacheline>){
                            cache.input.emplace_back(
                                /// 其实自动扩容逻辑还需要考虑一下
                                /// 因为未来可能会有运算重排导致比较大的cache先用上
                                /// 因此这里反而会出错
                                cache[val.index]
                            );
                        }
                    },t.data);
                }
                (it->second).func(cache.input,cache[op.output_index]);
                latest = op.output_index;
            }

            return cache[latest];
        }
    };
};


#endif 