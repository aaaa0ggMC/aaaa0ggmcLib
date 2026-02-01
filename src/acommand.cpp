#include <alib5/acommand.h>

using namespace alib5;

std::string_view Command::CommandInput::validate(const std::unordered_set<std::string_view> & allow) const noexcept{
    for(auto v : opts[depth]){
        if(allow.find(v) == allow.end())return v;
    }
    for(auto [k,v] : prefixes[depth]){
        if(allow.find(k) == allow.end())return k;
    }
    return "";
}

pvalue_t Command::CommandInput::get(std::string_view d) const noexcept{
    if(depth >= prefixes.size())return false;
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

std::pmr::vector<Command::CommandOutput> Command::__dispatch(bool rm){
    std::pmr::vector<std::pmr::unordered_set<std::string_view>> find_opts { ALIB5_DEFAULT_MEMORY_RESOURCE };
    std::pmr::vector<std::pmr::unordered_map<std::string_view,pvalue_t>> find_prefixes { ALIB5_DEFAULT_MEMORY_RESOURCE };
    std::pmr::vector<CommandOutput> cs ( ALIB5_DEFAULT_MEMORY_RESOURCE );

    Router::DispatchResult result = router.match<
        true,false
    >(parser,
    [&](pcursor_t * p){
        return judge_fn(p,find_opts.emplace_back(
            std::move(std::pmr::unordered_set<std::string_view>(ALIB5_DEFAULT_MEMORY_RESOURCE))
        ),find_prefixes.emplace_back(
            std::move(std::pmr::unordered_map<std::string_view,pvalue_t>(ALIB5_DEFAULT_MEMORY_RESOURCE))
        ));
    },
    [&](panalyser_t * ana){
        std::pmr::unordered_set<std::string_view> * last_opts;
        std::pmr::unordered_map<std::string_view,pvalue_t> * last_pfx;
        
        if(find_opts.size())last_opts = &find_opts.back();
        else last_opts = &find_opts.emplace_back(
            std::move(std::pmr::unordered_set<std::string_view>(ALIB5_DEFAULT_MEMORY_RESOURCE))
        );

        if(find_prefixes.size())last_pfx = &find_prefixes.back();
        else last_pfx = &find_prefixes.emplace_back(
            std::move(std::pmr::unordered_map<std::string_view,pvalue_t>(ALIB5_DEFAULT_MEMORY_RESOURCE))
        );

        for(auto & v : registered_prefixes){
            auto vec = ana->extract_options(v,"=",0);
            if(vec.size()){
                // 取最后一个
                last_pfx->emplace(
                    v,
                    vec.back()
                );
            }
            
        }

        for(auto &v : registered_options){
            // 可能是option
            auto has = ana->extract_flag(v,0);
            if(has){
                // 取最后一个
                last_opts->emplace(
                    v
                );
            }
        }
    },
    rm);

    if(result.dispatches.size() == 0){
        result.dispatches.push_back(0);
    }
    for(size_t i = 0;i < result.dispatches.size();++i){
        std::function<CommandOutput(const CommandInput &)> * func = nullptr;
        if(result.dispatches[i].id){
            auto m = dispatchers.find(result.dispatches[i].id);
            if(m != dispatchers.end())func = &m->second;
        }else if(default_dispatcher){
            func = &default_dispatcher;
        }

        if(func){
            CommandOutput co = (*func)(
                CommandInput{
                    (i+1 == result.dispatches.size()),
                    i,
                    find_opts,
                    find_prefixes,
                    defaults,
                    result.keys,
                    result.remains,
                    *this
                }
            );
            if(co){
                cs.emplace_back(co);
            }
            if(co.should_terminate)break;
        }
    }
    if(cs.empty())cs.emplace_back(CommandOutput::no_output());
    return cs;
}

panalyser_t Command::judge_fn(pcursor_t * cursor,std::pmr::unordered_set<std::string_view> & o,std::pmr::unordered_map<std::string_view,pvalue_t> & p){
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