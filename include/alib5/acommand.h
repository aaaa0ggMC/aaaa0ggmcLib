/**@file acommand.h
* @brief 简单的命令行解释器
* @author aaaa0ggmc
* @date 2026/02/03
* @version 5.0
* @copyright Copyright(c) 2026 
*/
#ifndef ALIB5_ACOMMAND
#define ALIB5_ACOMMAND
#include <alib5/arouter.h>
#include <alib5/alogger.h>
#include <alib5/ecs/linear_storage.h>
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
        std::pmr::unordered_set<
            std::pmr::string,
            detail::TransparentStringHash,
            detail::TransparentStringEqual
        > registered_options;
        /// 支持的prefix,默认所有的opt_str为"="
        std::pmr::unordered_set<std::pmr::string> registered_prefixes;
        /// 支持的默认值,key直接引用registered_prefixes
        std::pmr::unordered_map<std::string_view, pvalue_t> defaults;
        // 这个是用于保存value的池子
        std::pmr::unordered_set<std::pmr::string> default_values;
        /// 介绍
        std::pmr::unordered_map<
            std::pmr::string, 
            std::pmr::string,
            detail::TransparentStringHash,
            detail::TransparentStringEqual
        > descriptions;

        struct CommandInput{
            bool main;
            size_t depth;
            std::pmr::vector<std::pmr::unordered_set<std::string_view>> & opts;
            std::pmr::vector<std::pmr::unordered_map<std::string_view,pvalue_t>> & prefixes;
            std::pmr::unordered_map<std::string_view, pvalue_t> & defaults;
            std::pmr::unordered_map<
                std::pmr::string,
                std::pmr::string,
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > & keys;
            std::span<std::string_view> remains;
            std::span<std::string_view> routes;
            Command & cmd;

            /// 进行二次校验
            ALIB5_API std::string_view validate(const std::unordered_set<std::string_view> & allow) const noexcept;

            /// 获取value,depth为层次,小于0定位当前层级,大于最大层次返回pvalue_t(false)(invalid)
            ALIB5_API pvalue_t get(std::string_view d,int depth = -1) const noexcept;

            /// 获取更加多层次的数据,返回对应数值以及找到的对应层级
            inline std::pair<pvalue_t,int> gets(std::string_view d,int begin = -1,int end = std::numeric_limits<int>::max()){
                if(begin < 0)begin = depth;
                for(int i = begin;i < std::min(end,(int)prefixes.size());++i){
                    if(auto val = get(d,i)){
                        return {val,i};
                    }
                }
                return {false,-1};
            }

            /// 获取key
            inline pvalue_t key(std::string_view d) const noexcept{
                auto it = keys.find(d);
                if(it != keys.end()){
                    return it->second;
                }
                return false;
            }

            inline bool check(std::string_view d,int depth = -1) const noexcept{
                if(depth < 0)depth = this->depth;
                if(depth >= opts.size())return false;
                return opts[depth].find(d) != opts[depth].end();
            }

            /// 获取更加多层次的数据,返回对应数值以及找到的对应层级
            inline std::pair<bool,int> checks(std::string_view d,int begin = -1,int end = std::numeric_limits<int>::max()){
                if(begin < 0)begin = depth;
                for(int i = begin;i < std::min(end,(int)prefixes.size());++i){
                    if(check(d,i)){
                        return {true,i};
                    }
                }
                return {false,-1};
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

            static inline CommandOutput with_code(int code){
                CommandOutput co = code;
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
        template<bool has_color = false> struct ALIB5_API HelpMessage{
            Command & cmd;

            inline HelpMessage<true> color(){
                HelpMessage<true> h = { cmd };
                return h;
            }

            template<class CTX> CTX&& self_forward(CTX&& ctx) noexcept;
        };

        /// 生成帮助信息
        HelpMessage<false> help(){
            return HelpMessage<false>{*this };
        }

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
                registered_options.emplace(bundle.name);
                if(bundle.description != ""){
                    descriptions.emplace(bundle.name,bundle.description);
                }
            }
        }

        /// 注册options,但是有默认值
        void register_options(std::initializer_list<detail::BundleWithDefault> list) {
            auto res = ALIB5_DEFAULT_MEMORY_RESOURCE;
            for (auto const& bundle : list) {
                auto rp = registered_prefixes.emplace(bundle.name);
                if(bundle.value != ""){
                    registered_options.emplace(bundle.name);
                    defaults.emplace(*rp.first, *default_values.emplace(bundle.value).first);
                }
                if(bundle.description != ""){
                    descriptions.emplace(bundle.name,bundle.description);
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

namespace alib5{
    template<bool with_color> template<class CTX> inline CTX&& Command::HelpMessage<with_color>::self_forward(CTX&& ctx) noexcept{
        constexpr auto select_color = []<class T>(T && val){
            if constexpr(with_color){
                return std::forward<T>(val);
            }else{
                return log_nop{};
            }
        };
        
        static constexpr std::string_view STEP = "    ";
        static constexpr std::string_view BAR  = "│   ";
        static constexpr std::string_view TEE  = "├── ";
        static constexpr std::string_view ELB  = "└── ";
        constexpr auto c_lblue = select_color(LOG_COLOR1(LBlue));
        constexpr auto c_seps = select_color(LOG_COLOR3(Gray,None,Dim));
        constexpr auto c_reset = select_color(LOG_COLOR3(None,None,None));
        constexpr auto c_bwhite = select_color(LOG_COLOR3(White,None,Bold));
        constexpr auto c_yellow = select_color(LOG_COLOR1(Yellow));

        // 输出命令,DFS查询
        struct NN{
            RouterNode * val;
            std::string_view name;
        };
        std::deque<NN> nodes;
        ecs::detail::MonoticBitSet bits;
        
        for(auto & [key,value] : cmd.router.root.children){
            nodes.emplace_back(NN{&value,key});
        }

        int depth = 0;
        while(!nodes.empty()){
            NN n = nodes.back();
            nodes.pop_back();
            auto fn = [&]{
                std::move(ctx) << c_seps;
                for(int i = 0;i < depth -1;++i)
                    std::move(ctx) << (bits.get(i + 1) ? STEP : BAR);

                bits.ensure(depth + 1);
                if(depth >= 1){
                    if(nodes.empty() || nodes.back().val == nullptr){
                        std::move(ctx) << ELB;
                        bits.set(depth);
                    }else{
                        std::move(ctx) << TEE;
                        bits.reset(depth);
                    }
                }
                if(n.val->rule != RouterNode::Any){
                    std::move(ctx) << c_reset << 
                    (n.val->children.empty()?
                        c_lblue:
                        c_bwhite
                    )
                 << n.name << c_reset << "\n";
                }else{
                    std::move(ctx) << c_reset << c_yellow <<
                    "{" << n.name << "}" << c_reset << "\n";
                }
            };

            if(n.val == nullptr){
                --depth;
                continue;
            }

            fn();

            if(n.val->children.size()){
                nodes.emplace_back(NN{nullptr,""});
                /// 我觉得还是按照字母表排序吧
                std::size_t s1 = nodes.size();
                for(auto & [key,value] : (n.val)->children){
                    nodes.emplace_back(NN{&value,key});
                }
                std::sort(nodes.begin() + s1,nodes.end(),
                    [](const NN& a, const NN& b){
                        return a.name > b.name;
                    }
                );
                ++depth;
            }
        }
        // 移除多余的空行
        bool remove_more = true;
        
        if(cmd.registered_options.size()){
            remove_more = false;
            std::move(ctx) << c_bwhite << "\nSupport toggles:" << c_reset << "\n";
            std::vector<std::string_view> options;
            options.reserve(cmd.registered_options.size());
            int max_len = 0;
            for(auto & vv : cmd.registered_options){
                // 属于prefix
                if(cmd.registered_prefixes.find(vv) != cmd.registered_prefixes.end())continue;
                if(vv.size() > max_len)max_len = vv.size();
                options.emplace_back(vv);
            }

            std::sort(
                options.begin(),
                options.end(),
                [](const std::string_view& a, const std::string_view& b){
                    return a < b;
                }
            );
            
            
            max_len = max_len + 1;
            for(auto it = options.begin();it != options.end();){
                std::string_view desc = "";
                auto whit = cmd.descriptions.find(*it);
                if(whit != cmd.descriptions.end()){
                    desc = whit->second;       
                }
                std::move(ctx) << c_yellow << STEP << log_tfmt("{:<{}}") 
                << std::make_format_args(*it,max_len) << c_reset <<
                "| "
                << desc
                << ((++it == options.end())?"":"\n");
            }
        }

        if(cmd.registered_prefixes.size()){
            remove_more = false;
            std::move(ctx) << c_bwhite << "\nSupport options:" << c_reset << "\n";
            std::vector<std::string_view> prefixes;
            prefixes.reserve(cmd.registered_prefixes.size());
            int max_len = 0;
            int max_len_default = 0;
            for(auto & vv : cmd.registered_prefixes){
                if(vv.size() > max_len)max_len = vv.size();
                auto it = cmd.defaults.find(vv);
                if(it != cmd.defaults.end()){
                    if(it->second.data.length() > max_len_default)max_len_default = it->second.data.length();
                }
                prefixes.emplace_back(vv);
            }

            std::sort(
                prefixes.begin(),
                prefixes.end(),
                [](const std::string_view& a, const std::string_view& b){
                    return a < b;
                }
            );
            
            
            max_len = max_len + 1;
            max_len_default += 1;

            for(auto it = prefixes.begin();it != prefixes.end();){
                std::string_view desc = "";
                std::string_view defc = "";
                auto whit = cmd.descriptions.find(*it);
                if(whit != cmd.descriptions.end()){
                    desc = whit->second;       
                }
                auto def = cmd.defaults.find(*it);
                if(def != cmd.defaults.end()){
                    defc = def->second.data;
                }

                std::move(ctx) << c_yellow << STEP << log_tfmt("{:<{}}") 
                << std::make_format_args(*it,max_len) << c_reset <<
                "| " << log_tfmt("{:<{}}")
                << std::make_format_args(defc,max_len_default) <<
                "| "
                << desc
                << ((++it == prefixes.end())?"":"\n");
            }
        }
        
        if(remove_more)
            std::move(ctx) << log_erase(1);
        return std::move(ctx);
    }

}

#endif