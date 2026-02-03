/**@file arouter.h
* @brief 简单的路径分发器
* @author aaaa0ggmc
* @date 2026/02/03
* @version 5.0
* @copyright Copyright(c) 2026 
*/
#ifndef ALIB5_AROUTER
#define ALIB5_AROUTER
#include <alib5/aparser.h>

namespace alib5{
    template<class T> concept JudgeFn = requires(T&&t,pcursor_t & cursor){
        { t(&cursor) } -> std::convertible_to<panalyser_t>;
    };
    template<class T> concept RoamerFn = requires(T&&t,panalyser_t & cursor){
        { t(&cursor) } -> std::convertible_to<void>;
    };

    struct ALIB5_API RouterNode{
        /// 分发器，如果dispatcher id为0,表示此处没有dispatcher或者采取默认的dispatcher
        struct ALIB5_API Dispatcher{
            int64_t id;
            Dispatcher(int64_t i = 0):id(i){}
            operator int64_t(){return id;}
        };
        /// 此处路径的规则，目前分两种
        /// Any 表示此处随意匹配
        /// Full 表示此处匹配完整的name
        /// 对于any匹配，如果存在name则会把name视为匹配规则的id存储到dispatchresult的unordered_map中
        /// 如果不存在则不管
        /// 而对于command line的使用场景也没关系，除了每个层级的options匹配，dispatch_result还会返回一个
        /// remains analyser来告诉你还没有处理完毕的数据
        /// 那这里你就会问了，这个follow不是第三个吗？
        /// 其实这个follow只是拿给add_route使用的，实际的树中出现follow默认为Full
        /// 只有full和any
        /// 同时一层只允许一个any
        enum MatchRule{
            Follow,
            Full,
            Any
        };
        
        MatchRule rule { Follow };
        Dispatcher dispatcher { 0 };
        std::pmr::unordered_map<
            std::pmr::string,
            RouterNode,
            detail::TransparentStringHash, 
            detail::TransparentStringEqual
        > children;

        RouterNode(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE):children(__a){}
    
    private:
        RouterNode* any_cache { nullptr }; 
        std::string_view any_name { "" };
    public:
        // 树并没有提供delete函数的打算，因此很安全
        std::pair<std::string_view,RouterNode*> find_any(){
            if(any_cache){
                return {any_name,any_cache};
            }else{
                for(auto & [name,v] : children){
                    if(v.rule == Any){
                        any_cache = &v;
                        any_name = name;
                        return {any_name,&v};
                    }
                }
            }
            return {"",nullptr};
        }
    };

    struct ALIB5_API Router{
        RouterNode root;

        struct ALIB5_API DispatchResult{
            std::vector<RouterNode::Dispatcher> dispatches;
            std::pmr::unordered_map<
                std::pmr::string,
                std::pmr::string,    
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > keys;
            std::pmr::vector<panalyser_t> depth_analysers;
            std::pmr::vector<std::string_view> remains; // head为最后一个层级的起始子
            std::pmr::vector<std::string_view> routes;
        };

        struct MatchSettings{
            /// 是否生成{any}节点对应的k-v
            bool keys = true;
            /// 是否生成每层的解析器
            bool analyser = true;
            /// 是否生成最终剩下的数据
            bool remains = true;
            /// 是否保存走过的dispatcher
            bool dispatchers = true;
            /// 是否保存走过的路径到
            bool routes = true;
        };
        constexpr static MatchSettings default_match_settings = MatchSettings{
            true,true,true,true,true
        };
        
        struct Node{
            // 恰好routernode是长时间引用
            RouterNode * parent;

            /// 产生子节点
            Node ALIB5_API Group(std::string_view name,std::optional<RouterNode::Dispatcher> dispatch = std::nullopt);

            inline void dispatch(RouterNode::Dispatcher disp){
                panic_debug(!parent,"Invalid group context");
                [[unlikely]] if(!parent)return;
                parent->dispatcher = disp;
            }

            inline void rule(RouterNode::MatchRule rule){
                panic_debug(!parent,"Invalid group context");
                [[unlikely]] if(!parent)return;
                parent->rule = rule;
            }

            inline void operator=(RouterNode::Dispatcher disp){
                dispatch(disp);
            }

            template<class T> inline void operator=(T && v) requires
            requires(T & t){
                t.dispatch(*this);
            }{
                v.dispatch(*this);
            }
        };

        inline void clear(){
            root = RouterNode{};
        }

        inline Node Group(std::string_view name,std::optional<RouterNode::Dispatcher> dispatch = std::nullopt){
            return Node{&root}.Group(name,dispatch);
        }

        bool ALIB5_API add_route(std::string_view node_name,const RouterNode & nodes) noexcept;
        /// @param remove_head 是否不关心第一个元素,这个关系到树的正确索引
        template<
            MatchSettings settings = default_match_settings,
            JudgeFn Judge,
            RoamerFn Roamer
        >  DispatchResult match(panalyser_t parser,Judge && Fn,Roamer && Rn,bool remove_head) noexcept;
        
        template<
            MatchSettings settings = default_match_settings,
            JudgeFn Judge,
            RoamerFn Roamer
        > inline DispatchResult match(Parser & parser,Judge && Fn,Roamer && Rn,bool remove_head = true) noexcept{
            return match<settings>(parser.analyse(),std::forward<Judge>(Fn),std::forward<Roamer>(Rn),remove_head);
        }

        /// 实际上的版本
        bool ALIB5_API add_route(std::span<const std::string_view> path,RouterNode::Dispatcher dispatcher) noexcept;

        /// 通过类似网络的方式添加数据
        /// 比如 path = "aaaa/{bbb}/ccc",可以用于可视化构建树,括号包起来为any,没包起来为full 
        inline bool add_route(std::string_view path,RouterNode::Dispatcher dispatcher){
            size_t valid = 0;
            auto c = str::split(path,'/');
        
            for(auto v : c){
                if(v.empty())++valid;
                else break;
            }
            if(valid == c.size())return false;
            return add_route(std::span(c.begin() + valid,c.end()),dispatcher);
        }
    };

    using rdispatcher_t = RouterNode::Dispatcher;
    using rgroup_t = Router::Node;
}

namespace alib5{
    template<
        Router::MatchSettings conf,
        JudgeFn Judge,
        RoamerFn Roamer 
    > inline Router::DispatchResult Router::match(panalyser_t parser,Judge && Fn,Roamer && Rn,bool remove_head) noexcept{
        DispatchResult result;
        // 移除掉最开始的命令头
        // 一般为CLI的时候用到
        // 比如git remove XXX --> remove XXX
        // 这样的话就算程序改名字了还是可以锁定树的
        // 但是如果你是做带命令行的程序的话其实是不需要remove的
        if(remove_head)parser.inputs.erase(parser.inputs.begin());
        pcursor_t c = parser.as_cursor();
        RouterNode * focus = &root;

        // 如果早退,理论上parser.inputs.size() == 0,判断条件为 > 1,是合理的
        while(parser && c && !focus->children.empty()){
            auto token = c.head();
            auto it = focus->children.find(token.data);
            if(it != focus->children.end() && it->second.rule != RouterNode::Any){
                focus = &(it->second);
                if constexpr(conf.routes){
                    result.routes.emplace_back(it->first);
                }
            }else{
                // 可能存在any
                auto t = focus->find_any();
                if(t.second){
                    focus = t.second;
                    // 存在any对象
                    if constexpr(conf.keys){
                        result.keys.emplace(
                            t.first,
                            token.data
                        );
                    }
                    if constexpr(conf.routes){
                        result.routes.emplace_back(t.first);
                    }
                }else{
                    // 连any都找不到那不就是到这里就结束了吗?
                    break;
                }
            }
            if constexpr(conf.dispatchers){
                result.dispatches.push_back(focus->dispatcher);
            }
            // 找到了,同时装载数据
            // 这里应该是直接把一大块全部拿走了
            if constexpr(conf.analyser){
                result.depth_analysers.push_back(Fn(&c));
            }else Fn(&c);
            // 因为cursor不支持operator=复制,只支持构造复制
            if(c)c.commit();
            c.~Cursor();
            new (&c) pcursor_t(parser.as_cursor());
        }
        // 这里构造数据
        // if(c)c.commit();
        bool need_build = true;
        /// 一般来讲只有这个是异常
        if(parser.inputs.size() == 1){
            auto it = focus->children.find(parser.inputs[0]);
            RouterNode::Dispatcher disp;

            if(it != focus->children.end()){
                need_build = false;
                disp = it->second.dispatcher;
                if constexpr(conf.routes){
                    result.routes.emplace_back(it->first);
                }
            }else if(focus->find_any().second != nullptr){
                if constexpr(conf.keys){
                    result.keys.emplace(
                        focus->find_any().first,
                        parser.inputs[0]
                    );
                }
                disp = focus->find_any().second->dispatcher;
                need_build = false;
                if constexpr(conf.routes){
                    result.routes.emplace_back(focus->find_any().first);
                }
            }
            if(!need_build){
                auto cc = parser.as_cursor();
                if constexpr(conf.dispatchers){
                    result.dispatches.push_back(disp);
                }
                if constexpr(conf.analyser){
                    result.depth_analysers.emplace_back(Fn(&cc));
                }else Fn(&cc);
                if constexpr(conf.remains) {
                    result.remains = {};
                }
            }
        }
        if constexpr(conf.remains){
            if(need_build && parser.inputs.size()){
                parser.inputs.insert(
                    parser.inputs.begin(),"--REMAINS--");
                Rn(&parser);
                result.remains.insert(result.remains.begin(),parser.inputs.begin() + 1,parser.inputs.end());
            }
        }
        return result;
    }
}

#endif