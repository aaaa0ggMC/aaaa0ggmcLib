#ifndef ALIB5_AEVAL_EXECUTOR
#define ALIB5_AEVAL_EXECUTOR
#include <alib5/autil.h>
#include <alib5/ecs/linear_storage.h>
#include <alib5/aref.h>

namespace alib5::eval{
    template<class CharT = char,class ValueType = double >
    struct ALIB5_API ConstanceStorage{
        using key_t = std::pmr::basic_string<CharT>;
        using store_t = ecs::detail::LinearStorage<ValueType>;

        store_t values;
        std::pmr::unordered_map<
            key_t,
            size_t,
            alib5::detail::TransparentStringHash,
            alib5::detail::TransparentStringEqual    
        > mapper;

        ConstanceStorage(std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :values(0,allocator,allocator)
        ,mapper(allocator){}

        RefWrapper<store_t> operator[](std::basic_string_view<CharT> name){
            // 要求是支持ValueType();这个是整个eval库目前必需的
            return try_emplace(name);
        }

        template<class...Args>
        RefWrapper<store_t> try_emplace(std::basic_string_view<CharT> name,Args&&... args){
            auto it = mapper.find(name);
            if(it == mapper.end()){
                    bool flag;
                size_t index;
                values.try_next_with_index(flag, index,std::forward<Args>(args)...);
                mapper.emplace(
                    key_t(name,mapper.get_allocator()),
                    index
                );
                return ref(values,index);
            }else{
                return ref(values,it->second);
            }
        }

        bool remove(std::basic_string_view<CharT> name){
            auto it = mapper.find(name);
            if(it != mapper.end()){
                values.remove(it->second);
                mapper.erase(it);
                return true;
            }
            return false;
        }
        
        size_t get_id(std::basic_string_view<CharT> name) const {
            auto it = mapper.find(name);
            if(it != mapper.end())return it->second;
            return std::variant_npos;
        }

        std::optional<ValueType> get_copy(std::basic_string_view<CharT> name) const {
            auto it = mapper.find(name);
            if(it == mapper.end()){
                return std::nullopt;
            }
            return { values[it->second] };
        }

        std::optional<ValueType> get_copy(size_t index) const {
            if(values.available_bits.get(index)){
                return std::nullopt;
            }
            return { values[index] };
        }
    };

    // 一般都返回true
    enum Operation{
        /// 已经处理完毕,转发单值
        Processed,
        /// 也许你对values进行了修改,但是无论如何,这里将转发values给下一个处理对象
        Forward
    };
    template<class CharT = char,class ValueType = double >
    using Caller = std::function<Operation(ValueType & result,std::span<ValueType*> values)>;
 
    template<class CharT = char,class ValueType = double >
    struct Context{

    };

    template<class CharT = char,class ValueType = double >
    struct Executor{
        using call_t = Caller<CharT,ValueType>;
        struct Location{
            enum Loc{
                Input,
                Graph,
                Constance,
                Caching,
                Context
            };
            Loc location;
            size_t index;
        };
        
        struct Command{
            enum CmdType{
                Run,
                Store
            };
            CmdType type;
            size_t index; // 保存图信息的位置

            call_t function;
            std::pmr::vector<Location> values;
        };
        std::vector<Command> cmd;

        ValueType result_output;
        ValueType result_input;
        // 用于保存各种中间变量
        // caching是表达式中的各种数值,优化下会进行合并
        // 比如 1 + 1 + 1合并成 1
        // 所以十分不建议在span<ValueType*> input中修改参数数值,理论上确实可以这么做
        // 但是这是未定义的
        std::pmr::vector<ValueType> caching;
        std::pmr::vector<ValueType> graph;
        std::pmr::vector<ValueType> constance;
        Context<CharT,ValueType> & context;


        std::pmr::vector<ValueType*> execute_values;

        Executor(Context<CharT,ValueType> & c):context(c){}

        void add_cmd(call_t call,std::pmr::vector<Location> values){
            cmd.emplace_back(Command::Run,0,call,values);
        }

        // 需要保证在这段时间不操作caching,graph和constance,也不建议操作context
        std::optional<ValueType> execute(){
            result_input = ValueType();
            result_output = ValueType();
            
            for(Command & c : cmd){
                if(c.type == c.Run){
                    execute_values.clear();
                    // 计算数据指向
                    for(Location & loc : c.values){
                        switch(loc.location){
                        case loc.Input:
                            execute_values.emplace_back(&result_input);
                            break;
                        case loc.Caching:
                            if(loc.index >= caching.size()) [[unlikely]] {
                                return std::nullopt;
                            }
                            execute_values.emplace_back(&caching[loc.index]);
                            break;
                        case loc.Graph:
                            if(loc.index >= graph.size()) [[unlikely]] {
                                return std::nullopt;
                            }
                            execute_values.emplace_back(&graph[loc.index]);
                            break;
                        case loc.Constance:
                            if(loc.index >= constance.size()) [[unlikely]] {
                                return std::nullopt;
                            }
                            execute_values.emplace_back(&constance[loc.index]);
                            break;
                        case loc.Context:
                            return std::nullopt;
                        }
                    }
                    // c0存储当前的运算
                    // values中可能存在c1
                    c.function(result_output,execute_values);
                    // 把output回写进入input
                    result_input = std::move(result_output);
                }else if(c.type == c.Store){
                    if(graph.size() <= c.index)graph.resize(c.index + 1,ValueType());
                    graph[c.index] = std::move(result_input);
                    result_input = ValueType();
                    result_output = ValueType();
                }
            }
            return result_input;
        }

        void set_constance(const ConstanceStorage<CharT,ValueType> & c){
            constance = c.values.data;
        }
    };
}

#endif