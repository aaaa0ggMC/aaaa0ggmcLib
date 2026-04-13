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

    enum class SwapStrategy{
        NotSwappable,      // 整体都不可以交换 比如  a!b!c! 这种?
        Full,              // 整体可以交换    比如 a + b + c
        ExceptFirst,       // 第一个位置不可以交换 比如 a - b - c
    };

    struct OpData{
        std::string_view name;
        bool m_appendable { false };
        SwapStrategy m_swappable { SwapStrategy::NotSwappable };

        OpData& swappable(SwapStrategy v = SwapStrategy::Full){ m_swappable = v; return *this; }
        OpData& appendable(bool v = true){ m_appendable = v; return *this; }

        bool is_appendable(){
            return m_appendable;
        }
        SwapStrategy is_swappable(){
            return m_swappable;
        }

        OpData(std::string_view name,std::pmr::memory_resource * allocator)
        :name(name){}
    };

    template<IsValueType ValueType>
    struct SymbolData{
        std::string_view name;
        ValueType value { ValueType() };

        SymbolData(std::string_view name,std::pmr::memory_resource * allocator)
        :name(name){}
    };

    /// symbol走这个没啥问题,不过op我觉得可能不太建议走这个?
    template<
        IsValueType ValueType = double     
    >
    struct ALIB5_API Context{
        using TSymbolData = SymbolData<ValueType>;
    private:
        std::pmr::memory_resource * allocator;

        alib5::str::StringPool<std::pmr::string> pool;

        // --- Symbols ---
        // 记录symbol到底是什么,我就简单地使用string表示了
        // 其实symbol是连续分配的,本质上也没有啥
        std::pmr::vector<
            TSymbolData
        > symbol_strings;
        // 反向查询
        std::pmr::unordered_map<
            std::string_view,
            uint64_t
        > symbol_rev_mapper;

        // --- Ops ---
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
                TSymbolData(pool.get(name),allocator)
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

        TSymbolData& get_symbol_data(uint64_t id){ return const_cast<TSymbolData&>(((const Context&)(*this)).get_symbol_data(id)); }
        const TSymbolData& get_symbol_data(uint64_t id) const {
            [[unlikely]] panicf_if(
                id == 0 || id > symbol_strings.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                symbol_strings.size(),
                id
            );
            return symbol_strings[id - 1];
        }

        std::optional<TSymbolData*> get_symbol_data(std::string_view id){ 
            auto v = ((const Context&)(*this)).get_symbol_data(id); 
            if(v)return const_cast<TSymbolData*>(*v);
            else return std::nullopt;
        }
        std::optional<const TSymbolData*> get_symbol_data(std::string_view id) const {
            if(auto it = symbol_rev_mapper.find(id);it != symbol_rev_mapper.end()){
                return &symbol_strings[it->second - 1];
            }
            return std::nullopt;
        }

        OpData& get_op_data(uint64_t id){ return const_cast<OpData&>(((const Context&)(*this)).get_op_data(id)); }
        const OpData& get_op_data(uint64_t id) const {
            [[unlikely]] panicf_if(
                id == 0 || id > ops.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                ops.size(),
                id
            );
            return ops[id - 1];
        }

        std::optional<OpData*> get_op_data(std::string_view id){ 
            auto v = ((const Context&)(*this)).get_op_data(id); 
            if(v)return const_cast<OpData*>(*v);
            else return std::nullopt;
        }
        std::optional<const OpData*> get_op_data(std::string_view id) const {
            if(auto it = op_rev_mapper.find(id);it != op_rev_mapper.end()){
                return &ops[it->second - 1];
            }
            return std::nullopt;
        }
    };

    // 是整个操作里的值,包括变量,常量(数值的话另说)
    template<IsValueType ValueType = double >
    struct Symbol{
        using TContext = Context<ValueType>;

        TContext * ctx;
        // --- Symbol ID ---
        uint64_t id;

        Symbol(std::string_view name,TContext & ctx)
        :ctx(&ctx){
            auto i = ctx.emplace_symbol(name);
            this->id = i.second;
        }

        std::string_view get_name() const {
            return ctx->get_symbol_data(id).name;
        }

        ValueType& operator()(){
            return ctx->get_symbol_data(id).value;
        }

        const ValueType& operator()() const {
            return ctx->get_symbol_data(id).value;
        }

        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;
        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        template<class U>
        requires (!std::derived_from<std::decay_t<U>, Symbol<ValueType>>)
        Symbol& operator=(U && val){
            ctx->get_symbol_data(id).value = std::forward<U>(val);
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

    struct OCacheline{
        uint64_t index; // 不是id,这个从0开始
    };

    template<IsValueType ValueType = double >
    struct OOpTarget{
        std::variant<
            Symbol<ValueType>,
            OCacheline,
            ValueType
        > data;
    };

    struct OOp{
        uint64_t id;
    };

    template<IsValueType ValueType = double >
    struct Operation{
        using TSymbol = Symbol<ValueType>;

        // 实际上可以接受一堆op_targets吧
        // 反正至少有一个,因此直接使用左侧的context不就行了?
        // 默认的计算结果都写回cacheline0号位置?
        std::pmr::vector<OOpTarget<ValueType>> targets;
        OOp op;
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
        uint64_t child_cacheline_index = 0;
        
        void merge(
            const Expression<ValueType> & val,
            size_t id,
            bool add_after = true
        ){
            Expression<ValueType> v = val;
            merge(std::move(v),id,add_after);
        }

        void merge(
            Expression<ValueType> && val,
            size_t id,
            bool add_after = true
        ){
            uint64_t begin_index = child_cacheline_index + 1;
            uint64_t max_index = 0;
            for(Operation<ValueType> & op : val.operations){
                op.output_index += begin_index;
                if(op.output_index > max_index)max_index = op.output_index;

                for(OOpTarget<ValueType> & target : op.targets){
                    std::visit([&](auto & val){
                        using T = std::decay_t<decltype(val)>;
                        if constexpr(std::is_same_v<T,Symbol<ValueType>>){
                            // do nothing
                        }else if(std::is_same_v<T,OCacheline>){
                            val.index += begin_index;
                            if(val.index > max_index)max_index = val.index;
                        }
                    },target.data);
                }
            }
            child_cacheline_index = std::max(child_cacheline_index, max_index);
            this->operations.insert(
                this->operations.end(),
                std::make_move_iterator(val.operations.begin()),
                std::make_move_iterator(val.operations.end())
            );

            // 把最后的数据写入0中
            add({OCacheline{ begin_index }},id,add_after);
        }

        void add(
            OOpTarget<ValueType> a,
            size_t id,
            bool add_after = true
        ){
            // 这里引入简单的处理,如果前一个为相同id的op并且appendable那么进行合并
            if(!operations.empty() && 
               operations.back().op.id == id &&
               ctx.get_op_data(id).is_appendable()
            ){
                if(add_after){
                    operations.back().targets.emplace_back(
                        std::move(a)
                    );
                }else{
                    auto & t = operations.back().targets;
                    t.insert(
                        t.begin(),
                        std::move(a)
                    );              
                }
            }else{
                Operation<ValueType> op(ctx.get_allocator());
                op.output_index = 0;
                if(add_after){
                    op.targets.emplace_back(
                        OCacheline{ 0 }
                    );
                    op.targets.emplace_back(std::move(a));
                }else{
                    op.targets.emplace_back(std::move(a));
                    op.targets.emplace_back(
                        OCacheline{ 0 }
                    );
                }
                op.op.id = id;

                operations.emplace_back(
                    std::move(op)
                );
            }
        }

        Expression(TContext & ctx)
        :ctx(ctx){}
    };

    /// 一些生成expression的辅助函数
    template<class ValueType = double >
    Expression<ValueType> GenExpression(
        Context<ValueType> & ctx,
        OOpTarget<ValueType> a,
        OOpTarget<ValueType> b,
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

    template<IsValueType ValueType>
    struct RangeInfo{
        Symbol<ValueType> symbol;
        std::pmr::vector<ValueType> values;

        RangeInfo(std::pmr::memory_resource * alloc):values(alloc){}
    };

    template<IsValueType ValueType>
    struct DimensionalValue{
        size_t dimension_count;
        std::pmr::unordered_map<
            std::string_view,
            size_t 
        > dimension_names; // 从symbol name到dimension_lengths的映射
        std::pmr::vector<RangeInfo<ValueType>> dim_infos;
        std::pmr::vector<ValueType> values;

        struct When{

        };

        When when(const Symbol<ValueType> & symbol,const ValueType & value){
            
        }

        DimensionalValue(
            size_t dc,
            std::span<std::string_view> dim_names,
            std::span<RangeInfo<ValueType>> dim_infos,
            std::pmr::memory_resource * alloc
        )
        :dimension_count(dc)
        ,dimension_names(alloc)
        ,values(alloc){
            if(dim_infos.size()){
                size_t value_buffer_size = 1;
                this->dim_infos.insert(this->dim_infos.begin(),dim_infos.begin(),dim_infos.end());
                for(size_t i = 0;i < dim_infos.size();++i){
                    dimension_names.emplace(
                        dim_infos[i].symbol.get_name(),
                        i
                    );
                    value_buffer_size *= dim_infos[i].values.size();
                }
            }
        }
    };

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

        /// 进行范围求值
        struct Range{
        private:
            Executor & exec;
            std::pmr::unordered_map<std::string_view,RangeInfo<ValueType>>
                ranges_info;
        public:
            Range(Executor & exec)
            :exec(exec)
            ,ranges_info(exec.ctx.get_allocator()){}

            Range& discrete(const TSymbol & symbol,std::span<ValueType> values){
                RangeInfo<ValueType> & info = ranges_info.try_emplace(
                    symbol.get_name(),
                    RangeInfo<ValueType>(exec.ctx.get_allocator())
                ).first->second;
                info.values.clear();
                info.values.assign(info.values.begin().values.begin(),values.end());
            }

            template<class NextValueFn>
            Range& range(const TSymbol & symbol,ValueType begin,ValueType end,NextValueFn && next_value){
                RangeInfo<ValueType> & info = ranges_info.try_emplace(
                    symbol.get_name(),
                    RangeInfo<ValueType>(exec.ctx.get_allocator())
                ).first->second;
                info.values.clear();
                info.values.emplace_back(begin);
                ValueType v {};
                while((bool)next_value(v,info.values.back(),end)){
                    info.values.emplace_back(std::move(v));
                    v = ValueType();
                }
            }

            DimensionalValue<ValueType> execute(Expression<ValueType> & exp){
                TCacheLine cache(exec.ctx);
                return execute(exp,cache);
            }
            
            DimensionalValue<ValueType> execute(Expression<ValueType> & exp,TCacheLine & cache){
                DimensionalValue<ValueType> values(
                    ranges_info.size()
                );
            }
        };


        // 简单signal一下
        ValueType execute(const TExpression& exp,TCacheLine & cache){
            size_t latest = 0;

            cache.values.clear();
            cache.values.resize(exp.child_cacheline_index + 1 , ValueType() );
            // 先就是简单的执行吧
            for(const TOperation& op : exp.operations){
                auto it = funcs.find(op.op.id);
                if(it == funcs.end()){
                    // 先panic吧
                    panic("Invalid sequence.");
                }
                
                // 构建input
                cache.input.clear();
                for(const OOpTarget<ValueType> & t : op.targets){
                    std::visit([&](auto & val){
                        using T = std::decay_t<decltype(val)>;
                        if constexpr(std::is_same_v<T,TSymbol>){
                            cache.input.emplace_back(
                                ctx.get_symbol_data(val.id).value
                            );
                        }else if constexpr (std::is_same_v<T,OCacheline>){
                            cache.input.emplace_back(
                                /// 其实自动扩容逻辑还需要考虑一下
                                /// 因为未来可能会有运算重排导致比较大的cache先用上
                                /// 因此这里反而会出错
                                cache[val.index]
                            );
                        }else if constexpr (std::is_same_v<T,ValueType>){
                            cache.input.emplace_back(
                                val
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