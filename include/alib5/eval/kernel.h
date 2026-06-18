/**
 * @file kernel.h
 * @brief Simple parsing and evaluation library with symbols, ops, expressions and an executor. / 一个简单的解析运算库
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef ALIB5_AEVAL_KERNEL
#define ALIB5_AEVAL_KERNEL
#include <alib5/autil.h>
#include <alib5/adebug.h>

namespace alib5::eval{
    /// @brief 自动扩容的 cacheline 余量 / extra cacheline slots reserved for auto-expansion
    constexpr uint32_t conf_cacheline_auto_expand = 4;

    /**
     * @brief Concept constraining value types that are default-constructible and copy/move assignable.
     * @par Original Comment:
     * (无)
     */
    template<class T>
    concept IsValueType = requires(T & b,T & v){
        T();
        b = v;
        b = std::move(v);
    };

    /**
     * @brief Enumeration describing how operand order may be re-associated when building operations.
     * @par Original Comment:
     * (枚举值行内注释见下)
     */
    enum class SwapStrategy{
        NotSwappable,      // 整体都不可以交换 比如  a!b!c! 这种?
        Full,              // 整体可以交换    比如 a + b + c
        ExceptFirst,       // 第一个位置不可以交换 比如 a - b - c
    };

    /**
     * @brief Metadata describing a registered operator's name and reordering behavior.
     * @par Original Comment:
     * (无)
     */
    struct OpData{
        /// @brief 运算符名称 / operator name
        std::string_view name;
        /// @brief 是否可向同 op 追加 / whether the operator accepts appended operands
        bool m_appendable { false };
        /// @brief 可交换策略 / operand swap strategy
        SwapStrategy m_swappable { SwapStrategy::NotSwappable };

        /// @brief 设置可交换策略并返回 *this / set the swap strategy and return *this
        OpData& swappable(SwapStrategy v = SwapStrategy::Full){ m_swappable = v; return *this; }
        /// @brief 设置是否可追加并返回 *this / set appendable flag and return *this
        OpData& appendable(bool v = true){ m_appendable = v; return *this; }

        /// @brief 返回是否可追加 / return whether the operator is appendable
        bool is_appendable(){
            return m_appendable;
        }
        /// @brief 返回可交换策略 / return the configured swap strategy
        SwapStrategy is_swappable(){
            return m_swappable;
        }

        /// @brief 构造一个 OpData / construct an OpData with the given name
        OpData(std::string_view name,std::pmr::memory_resource * allocator)
        :name(name){}
    };

    /**
     * @brief Storage for a named symbol value held inside a Context.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType>
    struct SymbolData{
        /// @brief 符号名称 / symbol name
        std::string_view name;
        /// @brief 符号当前值 / current symbol value
        ValueType value { ValueType() };

        /// @brief 构造一个 SymbolData / construct a SymbolData with the given name
        SymbolData(std::string_view name,std::pmr::memory_resource * allocator)
        :name(name){}
    };

    /**
     * @brief Context owning the string pool, symbol table and operator table for expression construction.
     * @par Original Comment:
     * symbol走这个没啥问题,不过op我觉得可能不太建议走这个?
     */
    template<
        IsValueType ValueType = double
    >
    struct ALIB5_API Context{
        using TSymbolData = SymbolData<ValueType>;
    private:
        /// @brief 内存资源 / owned memory resource
        std::pmr::memory_resource * allocator;

        /// @brief 字符串池 / pooled string storage
        alib5::str::StringPool<std::pmr::string> pool;

        // --- Symbols ---
        // 记录symbol到底是什么,我就简单地使用string表示了
        // 其实symbol是连续分配的,本质上也没有啥
        /// @brief 符号表 / vector of symbol data
        std::pmr::vector<
            TSymbolData
        > symbol_strings;
        // 反向查询
        /// @brief 由 name 查询 id 的反向表 / reverse map from name to symbol id
        std::pmr::unordered_map<
            std::string_view,
            uint64_t
        > symbol_rev_mapper;

        // --- Ops ---
        /// @brief 运算符元数据表 / vector of operator metadata
        std::pmr::vector<
            OpData
        > ops;
        // 反向查询
        /// @brief 由 name 查询 op id 的反向表 / reverse map from name to operator id
        std::pmr::unordered_map<
            std::string_view,
            uint64_t
        > op_rev_mapper;

    public:
        /// @brief 构造 Context / construct a Context with an optional allocator
        Context(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :allocator(allocator)
        ,pool(allocator)
        ,symbol_strings(allocator)
        ,symbol_rev_mapper(allocator){}

        /// @brief 获取内部 allocator / return the allocator in use
        auto get_allocator() const { return allocator; }

        // 其实这个就是保证一个唯一性
        /// @note 注意id是从1开始的但是内存中存储是从0开始的
        /**
         * @brief Register a symbol by name (or return the existing entry) and return its name/id pair.
         * @param name 符号名称 / symbol name
         * @return 名称与从 1 开始的 id / name and 1-based id
         * @par Original Comment:
         * (见文件源码块注释)
         */
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
        /**
         * @brief Register an operator by name (or return the existing entry) and return its name/id pair.
         * @param name 运算符名称 / operator name
         * @return 名称与从 1 开始的 id / name and 1-based id
         * @par Original Comment:
         * (见文件源码块注释)
         */
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

        /// @brief 按 id 获取符号数据（可变） / return mutable symbol data by 1-based id
        TSymbolData& get_symbol_data(uint64_t id){ return const_cast<TSymbolData&>(((const Context&)(*this)).get_symbol_data(id)); }
        /**
         * @brief 按 id 获取符号数据（只读） / return const symbol data by 1-based id
         * @par Original Comment:
         * (无)
         */
        const TSymbolData& get_symbol_data(uint64_t id) const {
            [[unlikely]] panicf_if(
                id == 0 || id > symbol_strings.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                symbol_strings.size(),
                id
            );
            return symbol_strings[id - 1];
        }

        /// @brief 按 name 获取符号数据（可变） / return optional mutable symbol data by name
        std::optional<TSymbolData*> get_symbol_data(std::string_view id){
            auto v = ((const Context&)(*this)).get_symbol_data(id);
            if(v)return const_cast<TSymbolData*>(*v);
            else return std::nullopt;
        }
        /// @brief 按 name 获取符号数据（只读） / return optional const symbol data by name
        std::optional<const TSymbolData*> get_symbol_data(std::string_view id) const {
            if(auto it = symbol_rev_mapper.find(id);it != symbol_rev_mapper.end()){
                return &symbol_strings[it->second - 1];
            }
            return std::nullopt;
        }

        /// @brief 按 id 获取运算符元数据（可变） / return mutable operator data by 1-based id
        OpData& get_op_data(uint64_t id){ return const_cast<OpData&>(((const Context&)(*this)).get_op_data(id)); }
        /// @brief 按 id 获取运算符元数据（只读） / return const operator data by 1-based id
        const OpData& get_op_data(uint64_t id) const {
            [[unlikely]] panicf_if(
                id == 0 || id > ops.size(),
                "Index out of bounds!Expected [1,{}],but received {}.",
                ops.size(),
                id
            );
            return ops[id - 1];
        }

        /// @brief 按 name 获取运算符元数据（可变） / return optional mutable operator data by name
        std::optional<OpData*> get_op_data(std::string_view id){
            auto v = ((const Context&)(*this)).get_op_data(id);
            if(v)return const_cast<OpData*>(*v);
            else return std::nullopt;
        }
        /// @brief 按 name 获取运算符元数据（只读） / return optional const operator data by name
        std::optional<const OpData*> get_op_data(std::string_view id) const {
            if(auto it = op_rev_mapper.find(id);it != op_rev_mapper.end()){
                return &ops[it->second - 1];
            }
            return std::nullopt;
        }
    };

    // 是整个操作里的值,包括变量,常量(数值的话另说)
    /**
     * @brief Reference to a named symbol bound to a Context, providing read/write access to its value.
     * @par Original Comment:
     * (见文件源码块注释)
     */
    template<IsValueType ValueType = double >
    struct Symbol{
        using TContext = Context<ValueType>;

        /// @brief 所属 Context / owning Context
        TContext * ctx;
        // --- Symbol ID ---
        /// @brief 符号 id（从 1 开始） / 1-based symbol id
        uint64_t id;

        /// @brief 按 name 在 ctx 中注册并构造 Symbol / register the name in ctx and construct a Symbol
        Symbol(std::string_view name,TContext & ctx)
        :ctx(&ctx){
            auto i = ctx.emplace_symbol(name);
            this->id = i.second;
        }

        /// @brief 返回符号名称 / return the symbol name
        std::string_view get_name() const {
            return ctx->get_symbol_data(id).name;
        }

        /// @brief 返回符号当前值（可变） / return mutable reference to the symbol value
        ValueType& operator()(){
            return ctx->get_symbol_data(id).value;
        }

        /// @brief 返回符号当前值（只读） / return const reference to the symbol value
        const ValueType& operator()() const {
            return ctx->get_symbol_data(id).value;
        }

        Symbol(const Symbol&) = default;
        Symbol(Symbol&&) = default;
        Symbol& operator=(const Symbol&) = default;
        Symbol& operator=(Symbol&&) = default;

        /**
         * @brief Assign a non-Symbol value to the underlying symbol storage.
         * @par Original Comment:
         * (无)
         */
        template<class U>
        requires (!std::derived_from<std::decay_t<U>, Symbol<ValueType>>)
        Symbol& operator=(U && val){
            ctx->get_symbol_data(id).value = std::forward<U>(val);
            return *this;
        }
    };
    // 便利函数,说实话这个模板函数如果我转发的话代码量还大了点
    /**
     * @brief Convenience helper creating a tuple of Symbol handles from the given names.
     * @par Original Comment:
     * (见文件源码块注释)
     */
    template<IsValueType T = double ,IsStringLike... Ts>
    auto Symbols(Context<T> & ctx,Ts&&... symbols){
        return std::make_tuple(
            Symbol<T>(std::string_view(std::forward<Ts>(symbols)),ctx)...
        );
    }

    /**
     * @brief Cache line storing intermediate values during expression execution.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType = double >
    struct CacheLine{
        using TContext = Context<ValueType>;
        /// @brief 所属 Context / owning Context
        TContext & ctx;
        /// @brief 已计算值缓存 / cached computed values
        std::pmr::vector<ValueType> values;
        /// @brief 输入值缓存 / staging input buffer for the current operation
        std::pmr::vector<ValueType> input;

        /// @brief 构造 CacheLine / construct a CacheLine bound to the given Context
        CacheLine(TContext & ctx)
        :ctx(ctx){}

        /**
         * @brief Access the cached value at index, growing the buffer with default values when needed.
         * @par Original Comment:
         * (无)
         */
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

    /**
     * @brief Index into a CacheLine used as an operation target or output.
     * @par Original Comment:
     * (无)
     */
    struct OCacheline{
        uint64_t index; // 不是id,这个从0开始
    };

    /**
     * @brief Variant target describing a Symbol, CacheLine slot or literal value.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType = double >
    struct OOpTarget{
        std::variant<
            Symbol<ValueType>,
            OCacheline,
            ValueType
        > data;
    };

    /**
     * @brief Reference to a registered operator by id.
     * @par Original Comment:
     * (无)
     */
    struct OOp{
        uint64_t id;
    };

    /**
     * @brief Single operation recording its input targets, the invoked op and the output cacheline slot.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType = double >
    struct Operation{
        using TSymbol = Symbol<ValueType>;

        // 实际上可以接受一堆op_targets吧
        // 反正至少有一个,因此直接使用左侧的context不就行了?
        // 默认的计算结果都写回cacheline0号位置?
        /// @brief 输入目标列表 / input targets for the operation
        std::pmr::vector<OOpTarget<ValueType>> targets;
        /// @brief 调用的运算符 / operator to invoke
        OOp op;
        uint32_t output_index { 0 }; // 写入的位置,默认都是0,显然是可以调整的

        /// @brief 构造一个空 Operation / construct an empty Operation with the given allocator
        Operation(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :targets(allocator){}
    };

    /**
     * @brief Expression holding a sequence of operations executed against a shared Context and cacheline space.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType = double >
    struct ALIB5_API Expression{
        using TContext = Context<ValueType>;
        using TOperation = Operation<ValueType>;
        /// @brief 所属 Context / owning Context
        TContext & ctx;
        /// @brief 操作序列 / ordered list of operations
        std::pmr::vector<TOperation> operations;
        /// @brief 子表达式使用的最大 cacheline 索引 / highest cacheline index used by merged children
        uint64_t child_cacheline_index = 0;

        /**
         * @brief Copy-merge another expression into this one, adjusting cacheline indices.
         * @par Original Comment:
         * (无)
         */
        void merge(
            const Expression<ValueType> & val,
            size_t id,
            bool add_after = true
        ){
            Expression<ValueType> v = val;
            merge(std::move(v),id,add_after);
        }

        /**
         * @brief Move-merge another expression into this one, adjusting cacheline indices.
         * @par Original Comment:
         * (无)
         */
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
                        }else if constexpr(std::is_same_v<T,OCacheline>){
                            val.index += begin_index;
                            if(val.index > max_index)max_index = val.index;
                        }else if constexpr(std::is_same_v<T,ValueType>){
                            // do nothing
                        }else {
                            static_assert(std::is_same_v<T,Symbol<ValueType>>,"Implement this!");
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

        /**
         * @brief Append (or prepend) a target to the operation identified by id, merging when appendable.
         * @par Original Comment:
         * (见文件源码块注释)
         */
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

        /// @brief 构造绑定到 ctx 的空 Expression / construct an empty Expression bound to ctx
        Expression(TContext & ctx)
        :ctx(ctx){}

        // 输出指令集合
        /**
         * @brief Stream a human-readable dump of the operations into the given streaming context.
         * @par Original Comment:
         * (见文件源码块注释)
         */
        template<class TStreamedContext>
        TStreamedContext&& self_forward(TStreamedContext && ctx){
            for(const TOperation & op : operations){
                std::move(ctx)
                << this->ctx.get_op_data(op.op.id).name
                << " ";
                for(size_t i = 0;i < op.targets.size();++i){
                    std::visit([&](auto & val){
                        using T = std::decay_t<decltype(val)>;
                        if constexpr(std::is_same_v<T,Symbol<ValueType>>){
                            const Symbol<ValueType> & s = val;
                            std::move(ctx) << "\""
                            << s.get_name()
                            << "\"";
                        }else if constexpr(std::is_same_v<T,OCacheline>){
                            const OCacheline & s = val;
                            std::move(ctx) << "c" << s.index;
                        }else if constexpr (std::is_same_v<T,ValueType>){
                            std::move(ctx) << val;
                        }else {
                            static_assert(std::is_same_v<T,Symbol<ValueType>>,"Implement this!");
                        }
                    },op.targets[i].data);
                    if(i + 1 != op.targets.size())std::move(ctx) << ",";
                }

                std::move(ctx) << " -> " << "c" << op.output_index << "\n";
            }
            return std::move(ctx);
        }
    };

    /// 一些生成expression的辅助函数
    /**
     * @brief Build a simple two-operand Expression invoking the operator identified by id.
     * @par Original Comment:
     * (见文件源码块注释)
     */
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

    /**
     * @brief Storage for a symbol's discrete value range used during dimensional evaluation.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType>
    struct RangeInfo{
        /// @brief 对应符号 / owning symbol
        Symbol<ValueType> symbol;
        /// @brief 离散取值列表 / discrete values
        std::pmr::vector<ValueType> values;

        /// @brief 构造 RangeInfo / construct a RangeInfo with the given allocator
        RangeInfo(std::pmr::memory_resource * alloc):values(alloc){}
    };

    /**
     * @brief Multi-dimensional value table produced by ranging an Expression over several symbols.
     * @par Original Comment:
     * (无)
     */
    template<IsValueType ValueType>
    struct DimensionalValue{
        /// @brief 维度数量 / number of dimensions
        size_t dimension_count;
        std::pmr::unordered_map<
            std::string_view,
            size_t
        > dimension_names; // 从symbol name到dimension_lengths的映射
        /// @brief 每个维度的取值信息 / per-dimension range info
        std::pmr::vector<RangeInfo<ValueType>> dim_infos;
        /// @brief 展平后的结果值 / flattened result values
        std::pmr::vector<ValueType> values;

        struct When{

        };

        /**
         * @brief Placeholder API for filtering by a specific symbol/value pair.
         * @par Original Comment:
         * (无)
         */
        When when(const Symbol<ValueType> & symbol,const ValueType & value){

        }

        /**
         * @brief Construct a DimensionalValue from the given dimension names and range info.
         * @par Original Comment:
         * (无)
         */
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

    /**
     * @brief Executor that binds operator implementations to a Context and evaluates Expressions.
     * @par Original Comment:
     * (无)
     */
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
        /// @brief 所属 Context / owning Context
        TContext & ctx;

        /**
         * @brief Internal record storing the callable implementing an operator.
         * @par Original Comment:
         * (无)
         */
        struct OpData{
            /// @brief 运算符实现 / operator implementation
            OpFunc func;
        };

        /// @brief 运算符 id 到实现的映射 / map from operator id to its implementation
        std::pmr::unordered_map<
            uint64_t,
            OpData
        > funcs;

        /// @brief 构造绑定到 ctx 的 Executor / construct an Executor bound to ctx
        Executor(TContext & ctx)
        :ctx(ctx){}

        /**
         * @brief Register an operator implementation under the given name.
         * @par Original Comment:
         * (无)
         */
        void emplace_operator(std::string_view name,OpFunc fn){
            OpData & data = funcs[ctx.emplace_op(name).second];
            data.func = std::move(fn);
        }

        /**
         * @brief Evaluate the expression using a fresh CacheLine.
         * @par Original Comment:
         * (无)
         */
        auto execute(const TExpression & exp){
            CacheLine cache(ctx);
            return execute(exp,cache);
        }

        /// 进行范围求值
        /**
         * @brief Helper tracking per-symbol ranges for dimensional evaluation.
         * @par Original Comment:
         * (见文件源码块注释)
         */
        struct Range{
        private:
            /// @brief 所属 Executor / owning Executor
            Executor & exec;
            /// @brief 每个 symbol 的取值范围 / per-symbol range info
            std::pmr::unordered_map<std::string_view,RangeInfo<ValueType>>
                ranges_info;
        public:
            /// @brief 构造绑定到 exec 的 Range / construct a Range bound to exec
            Range(Executor & exec)
            :exec(exec)
            ,ranges_info(exec.ctx.get_allocator()){}

            /**
             * @brief Associate a symbol with a discrete set of values.
             * @par Original Comment:
             * (无)
             */
            Range& discrete(const TSymbol & symbol,std::span<ValueType> values){
                RangeInfo<ValueType> & info = ranges_info.try_emplace(
                    symbol.get_name(),
                    RangeInfo<ValueType>(exec.ctx.get_allocator())
                ).first->second;
                info.values.clear();
                info.values.assign(info.values.begin().values.begin(),values.end());
            }

            /**
             * @brief Generate values for a symbol using a user-supplied next-value function.
             * @par Original Comment:
             * (无)
             */
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

            /// @brief 在新的 cacheline 上对 exp 进行范围求值 / evaluate exp over the registered ranges using a fresh CacheLine
            DimensionalValue<ValueType> execute(Expression<ValueType> & exp){
                TCacheLine cache(exec.ctx);
                return execute(exp,cache);
            }

            /**
             * @brief Evaluate exp over the registered ranges using the provided CacheLine.
             * @par Original Comment:
             * (无)
             */
            DimensionalValue<ValueType> execute(Expression<ValueType> & exp,TCacheLine & cache){
                DimensionalValue<ValueType> values(
                    ranges_info.size()
                );
            }
        };


        // 简单signal一下
        /**
         * @brief Execute the expression against the given CacheLine and return the final result value.
         * @par Original Comment:
         * (见文件源码块注释)
         */
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
                        }else {
                            static_assert(std::is_same_v<T,Symbol<ValueType>>,"Implement this!");
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
