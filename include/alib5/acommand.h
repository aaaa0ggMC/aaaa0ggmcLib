/**@file acommand.h
* @brief 简单的命令行解释器
* @author aaaa0ggmc
* @date 2026/02/01
* @version 5.0
* @copyright Copyright(c) 2026 
*/
#ifndef ALIB5_ACOMMAND
#define ALIB5_ACOMMAND
#include <alib5/arouter.h>
#include <unordered_set>

namespace alib5{
    namespace detail{
        struct BundleWithDefault{
            std::string_view name;
            std::string_view value {"" };
            std::string_view description {""};
        };
        struct Toggle{ 
            std::string_view name;
            std::string_view description {""};
        };
        template<typename T>
        concept IsBundle = std::same_as<std::remove_cvref_t<T>, BundleWithDefault>;

        template<class T,class Cout,class Cin>
        concept IsRouteHandler = requires(T & t,const Cin & in){
            { t(in) } -> std::convertible_to<Cout>;
        };
    };

    /// 支持读取命令
    /// 命令的形式比较古板(因为我不想写复杂的judger了)
    /// 就是你去准备一个池子给我就行了
    struct ALIB5_API Command{
        /// 内部的parser
        Parser parser;
        /// 内部的索引
        Router router;
        /// 支持的选项
        std::pmr::unordered_set<std::pmr::string> registered_options;
        /// 支持的prefix,默认所有的opt_str为"="
        std::pmr::unordered_set<std::pmr::string> registered_prefixes;
        /// 支持的默认值,key直接引用registered_prefixes
        std::pmr::unordered_map<std::string_view, pvalue_t> defaults;
        // 这个是用于保存value的池子
        std::pmr::unordered_set<std::pmr::string> default_values;
        /// 介绍
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> descriptions;

        struct CommandInput{
            bool main;
            size_t depth;
            std::pmr::vector<std::pmr::unordered_set<std::string_view>> & opts;
            std::pmr::vector<std::pmr::unordered_map<std::string_view,pvalue_t>> & prefixes;
            std::pmr::unordered_map<std::string_view, pvalue_t> & defaults;
            std::pmr::unordered_map<std::pmr::string,std::pmr::string> & keys;
            std::span<std::string_view> remains;

            /// 进行二次校验
            ALIB5_API std::string_view validate(const std::unordered_set<std::string_view> & allow) const noexcept;

            /// 获取value
            ALIB5_API pvalue_t get(std::string_view d) const noexcept;

            /// 获取key
            inline pvalue_t key(std::string_view d) const noexcept{
                auto it = keys.find(std::pmr::string(d,ALIB5_DEFAULT_MEMORY_RESOURCE));
                if(it != keys.end()){
                    return it->second;
                }
                return false;
            }

            inline bool check(std::string_view d) const noexcept{
                if(depth >= opts.size())return false;
                return opts[depth].find(d) != opts[depth].end();
            }
        };

        struct CommandOutput{
            int code { 0 };
            bool valid { true };
            bool should_terminate { false };

            CommandOutput(bool v){
                panic_debug(v, "Why return true to signal this output is empty?");
                valid = false;
            }
            CommandOutput(int v){
                code = v;
            }
            operator bool(){return valid;}
        
            static inline CommandOutput terminate(int code = 0){
                CommandOutput co = code;
                co.valid = true;
                co.should_terminate = true;
                return co;
            }

            static inline CommandOutput no_output(){
                return false;
            }
        };

        /// dispatcher
        std::unordered_map<int64_t,std::function<CommandOutput(const CommandInput &)>> dispatchers;
        std::function<CommandOutput(const CommandInput &)> default_dispatcher;
        
    private:
        size_t dispatch_max_id {0};

        /// 实际上我们的analyser用不到depth_analysers,因为是线性的直接搞下去就行了
        panalyser_t ALIB5_API judge_fn(pcursor_t * cursor,std::pmr::unordered_set<std::string_view> & o,std::pmr::unordered_map<std::string_view,pvalue_t> & p);

        /// 分发命令给函数
        std::pmr::vector<CommandOutput> ALIB5_API __dispatch(bool rm);

        /// 注册函数
        inline rdispatcher_t __register_handler(std::function<CommandOutput(const CommandInput &)> fn){
            int64_t id = ++dispatch_max_id;
            dispatchers[id] = std::move(fn);
            return id;
        }
        inline rdispatcher_t __register_default_handler(std::function<CommandOutput(const CommandInput&)> fn){
            // dispatchers[0] = fn;
            default_dispatcher = std::move(fn);
            return 0;
        }
    public:

        template<detail::IsRouteHandler<CommandOutput,CommandInput> Fn> 
        inline rdispatcher_t register_handler(Fn && f){
            if constexpr (std::is_same_v<std::remove_cvref_t<Fn>, std::function<CommandOutput(const CommandInput&)>>) {
                return __register_handler(f);
            }else{
                return __register_handler(
                    [func = std::forward<Fn>(f)](const CommandInput & in) mutable -> CommandOutput {
                        return func(in);
                    }
                );
            }
        }

        template<detail::IsRouteHandler<CommandOutput,CommandInput> Fn> 
        inline rdispatcher_t register_default_handler(Fn && f){
            if constexpr (std::is_same_v<std::remove_cvref_t<Fn>, std::function<CommandOutput(const CommandInput&)>>) {
                return __register_default_handler(f);
            }else{
                return __register_default_handler(
                    [func = std::forward<Fn>(f)](const CommandInput & in) mutable -> CommandOutput {
                        return func(in);
                    }
                );
            }
        }

        /// 注册option
        void register_toggles(std::initializer_list<detail::Toggle> list) {
            auto res = ALIB5_DEFAULT_MEMORY_RESOURCE;
            for (auto const& bundle : list) {
                std::pmr::string pname(bundle.name, res);
                registered_options.emplace(pname);
                if(bundle.description != ""){
                    descriptions.emplace(pname,std::pmr::string(bundle.description,ALIB5_DEFAULT_MEMORY_RESOURCE));
                }
            }
        }

        /// 注册options,但是有默认值
        void register_options(std::initializer_list<detail::BundleWithDefault> list) {
            auto res = ALIB5_DEFAULT_MEMORY_RESOURCE;
            for (auto const& bundle : list) {
                std::pmr::string pname(bundle.name, res);
                auto rp = registered_prefixes.emplace(pname);
                if(bundle.value != ""){
                    std::pmr::string pval(bundle.value, res);
                    registered_options.emplace(pname);
                    defaults.emplace(*rp.first, *default_values.emplace(std::move(pval)).first);
                }
                if(bundle.description != ""){
                    descriptions.emplace(pname,std::pmr::string(bundle.description,ALIB5_DEFAULT_MEMORY_RESOURCE));
                }
            }
        }

        inline rgroup_t Group(std::string_view n,std::optional<rdispatcher_t> disp = std::nullopt){
            return router.Group(n,disp);
        }

        inline void clear_routes(){
            router.clear();
        }

        /// 语法,类似网络,比如"server/start",可以支持any如"sever/{id}/start"
        /// 支持转移语序比如"server/\n/\{id\}"(这里的\{\}就表示真的是{id}而不是作为any匹配)
        inline bool add_route(std::string_view path,rdispatcher_t disp){
            return router.add_route(path,disp);
        }

        /// 如果你有分开的需求也可以这么做
        inline bool add_route(std::span<const std::string_view> paths,rdispatcher_t disp){
            return router.add_route(paths,disp);
        }
        /// 语法,类似网络,比如"server/start",可以支持any如"sever/{id}/start"
        /// 支持转移语序比如"server/\n/\{id\}"(这里的\{\}就表示真的是{id}而不是作为any匹配)
        template<detail::IsRouteHandler<CommandOutput,CommandInput> Fn> 
        inline bool add_route(std::string_view path,Fn && fn){
            return router.add_route(path,register_handler(std::forward<Fn>(fn)));
        }

        /// 如果你有分开的需求也可以这么做
        template<detail::IsRouteHandler<CommandOutput,CommandInput> Fn> 
        inline bool add_route(std::span<const std::string_view> paths,Fn && fn){
            return router.add_route(paths,register_handler(std::forward<Fn>(fn)));
        }

        /// 从字符串中解析,这里我们默认不remove_head
        inline std::pmr::vector<CommandOutput> from_str(std::string_view commands){
            parser.parse(commands);
            return __dispatch(false);
        }
        /// 从系统提供的参数列表中解析,这里我们默认remove_head
        inline std::pmr::vector<CommandOutput> from_args(int argc,const char ** argv){
            parser.from_args(argc, argv);
            return __dispatch(true);
        }
    };

    using command_in_t = const Command::CommandInput &;
    using command_out_t = Command::CommandOutput;
};

#endif