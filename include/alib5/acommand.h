/**@file aparser.h
* @brief 简单的命令行解释器
* @author aaaa0ggmc
* @date 2026/01/31
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

            inline std::string_view validate(const std::unordered_set<std::string_view> & allow) const noexcept{
                for(auto v : opts[depth]){
                    if(allow.find(v) == allow.end())return v;
                }
                for(auto [k,v] : prefixes[depth]){
                    if(allow.find(k) == allow.end())return k;
                }
                return "";
            }

            inline pvalue_t get(std::string_view d) const noexcept{
                auto it = prefixes[depth].find(d);
                if(it != prefixes[depth].end())return it->second;
                // 如果只是检测option的话
                if(opts[depth].contains(d)){
                    auto it3 = defaults.find(d);
                    if(it3 != defaults.end())return it3->second;
                    return "";
                }
                return false;
            }

            inline bool check(std::string_view d){
                return opts[depth].find(d) != opts[depth].end();
            }

        };

        struct CommandOutput{
            bool valid;

            CommandOutput(bool v){valid = false;}
            operator bool(){return valid;}
        };

        /// dispatcher
        std::unordered_map<int64_t,std::function<CommandOutput(const CommandInput &)>> dispatchers;
    private:
        size_t dispatch_max_id {0};

        /// 实际上我们的analyser用不到depth_analysers,因为是线性的直接搞下去就行了
        inline panalyser_t judge_fn(pcursor_t * cursor,std::pmr::unordered_set<std::string_view> & o,std::pmr::unordered_map<std::string_view,pvalue_t> & p){
            pvalue_t nval = "";
            std::pmr::string td(ALIB5_DEFAULT_MEMORY_RESOURCE);
            while(*cursor && (nval = cursor->peek())){
                td = nval.data;
                /// 标记一下bool就行
                /// 这个流程意味着如果你想要prefix有默认值
                /// 可以往opt和prefix中塞入同样的字符串
                bool fnd = false;
                for(auto & v : registered_prefixes){
                    if(td.starts_with(v)){
                        if(td.size() > v.size()  && td[v.size()] != '='){
                            continue;
                        }
                        cursor->next();
                        p.emplace(
                            v,
                            cursor->next_value_bundle("=").second
                        );
                        fnd = true;
                        break;
                    }
                }
                if(fnd)continue;

                auto it1 = registered_options.find(td);
                if(it1 != registered_options.end()){
                    o.emplace(*it1);
                    cursor->next();
                    continue;
                }
                
                /// 什么都没抓到,退出
                break;
            }
            if(*cursor)return cursor->commit();
            return cursor->invalid();
        }

        inline std::pmr::vector<CommandOutput> __dispatch(bool rm){
            std::pmr::vector<std::pmr::unordered_set<std::string_view>> find_opts { ALIB5_DEFAULT_MEMORY_RESOURCE };
            std::pmr::vector<std::pmr::unordered_map<std::string_view,pvalue_t>> find_prefixes { ALIB5_DEFAULT_MEMORY_RESOURCE };
            std::pmr::vector<CommandOutput> cs ( ALIB5_DEFAULT_MEMORY_RESOURCE );

            Router::DispatchResult result = router.match(parser,
            [&](pcursor_t * p){
                return judge_fn(p,find_opts.emplace_back(
                    std::move(std::pmr::unordered_set<std::string_view>(ALIB5_DEFAULT_MEMORY_RESOURCE))
                ),find_prefixes.emplace_back(
                    std::move(std::pmr::unordered_map<std::string_view,pvalue_t>(ALIB5_DEFAULT_MEMORY_RESOURCE))
                ));
            },rm);
            
            for(size_t i = 0;i < result.dispatches.size();++i){
                auto m = dispatchers.find(result.dispatches[i].id);
                if(m != dispatchers.end()){
                    CommandOutput co = (m->second)(
                        CommandInput{
                            (i+1 == result.dispatches.size()),
                            i,
                            find_opts,
                            find_prefixes,
                            defaults,
                            result.keys,
                            result.remains
                        }
                    );
                    if(co){
                        cs.emplace_back(co);
                    }
                }
            }
            if(cs.empty())cs.emplace_back(false);
            return cs;
        }
    public:
        /// 注册函数
        inline rdispatcher_t register_handler(std::function<CommandOutput(const CommandInput &)> fn){
            int64_t id = ++dispatch_max_id;
            dispatchers[id] = fn;
            return id;
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

};

#endif