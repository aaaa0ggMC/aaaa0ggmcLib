#include <alib5/arouter.h>
#include <deque>

using namespace alib5;

Router::Node Router::Node::Group(std::string_view name,std::optional<RouterNode::Dispatcher> dispatch){
    panic_debug(!parent,"Invalid group context");
    [[unlikely]] if(!parent)return {NULL};
    if(name.empty())return {parent};

    auto rulep = RouterNode::Full;
    RouterNode * n = nullptr;
    if(name[0] == '{' && name.back() == '}'){
        name = name.substr(1,name.size()-2);
        rulep = RouterNode::Any;
    }
    std::pmr::string pname (str::unescape(name),ALIB5_DEFAULT_MEMORY_RESOURCE);
    auto it = parent->children.find(pname);
    if(it == parent->children.end()){
        n = &parent->children.emplace(pname,RouterNode{}).first->second;
        n->rule = rulep;
    }else{
        n = &it->second;
    }
    if(dispatch)n->dispatcher = *dispatch;
    return {n};
}

bool Router::add_route(std::span<const std::string_view> path,RouterNode::Dispatcher dispatcher) noexcept {
    if(path.empty())return false;
    RouterNode root;
    auto name = path[0];
    if(name.empty())return false;
    if(name[0] == '{' && name.back() == '}'){
        name = name.substr(1,name.size() - 2);
        root.rule = RouterNode::Any;
    }else root.rule = RouterNode::Follow;
    RouterNode * focus = &root;
    
    for(auto d : path.subspan(1)){
        [[unlikely]] if(d.empty())continue;
        RouterNode::MatchRule rule = RouterNode::Follow;

        if(d[0] == '{' && d.back() == '}'){
            d = d.substr(1,d.size() - 2);
            rule = RouterNode::Any;
        }
        
        focus = &focus->children.emplace(
            str::unescape(d),
            RouterNode{}
        ).first->second;

        focus->rule = rule;
    }

    focus->dispatcher = dispatcher;

    return add_route(str::unescape(name),root);
}

bool Router::add_route(std::string_view node_name,const RouterNode & nodes) noexcept {
    std::deque<const RouterNode*> current_layer;
    std::deque<const RouterNode*> next_layer;

    std::deque<RouterNode*> appender;
    std::deque<RouterNode*> next_appender;
    {
        RouterNode * current_focus = nullptr;
        /// 先检测是否已经有children了
        for(auto &[name,node] : root.children){
            if(name == node_name){
                current_focus = &node;
                if(nodes.rule != RouterNode::Follow && nodes.rule != node.rule){
                    /// 规则冲突
                    invoke_error(err_conflict_router,"Conflict root route rule {}",node_name);
                    return false;
                }
                if(nodes.rule != RouterNode::Follow)current_focus->dispatcher = nodes.dispatcher;
                break;
            }
        }
        if(!current_focus){
            // 既然是全新node，直接覆盖所有规则就行了
            current_focus = 
                &(root.children.emplace(
                    std::pmr::string(node_name,ALIB5_DEFAULT_MEMORY_RESOURCE),
                    nodes
                ).first->second);
        }
        appender.push_back(current_focus);
    }

    /// 标记作用，意味着到了下一个appender的领域
    current_layer.push_back(nullptr);
    current_layer.push_back(&nodes);
    
    while(!current_layer.empty()){
        RouterNode * current = nullptr;
        while(!current_layer.empty()){
            auto val = current_layer.front();
            current_layer.pop_front();
            // 换层了，同时防止出现意外
            if(val == nullptr){
                if(!appender.empty()){
                    current = appender.front();
                    appender.pop_front();
                }
                continue;
            }
            
            bool has_any = current->find_any().second != nullptr;

            for(auto & [name,t] : val->children){
                auto it = current->children.find(name);
                if(it == current->children.end()){
                    if(t.rule == RouterNode::Any && has_any){
                        invoke_error(err_conflict_router,"A node would have 2 ANY leaves,which is forbidden! {}",name);
                        return false;
                    }
                    // 这是一个新的节点，直接照着emplace就行
                    auto & v = current->children.emplace(
                        std::pmr::string(name,ALIB5_DEFAULT_MEMORY_RESOURCE),
                        t
                    ).first->second;
                }else{
                    // 这是已经存在的节点，检查规则是否冲突
                    RouterNode & r = it->second;
                    if(t.rule != RouterNode::Follow && r.rule != t.rule){
                        invoke_error(err_conflict_router,"Conflict route rule {}",name);
                        return false;
                    }
                    if(t.rule != RouterNode::Follow)r.dispatcher = t.dispatcher;
                    // 不冲突的话就可以迭代下一层级了
                    next_appender.push_back(&r);
                    next_layer.push_back(nullptr);
                    next_layer.push_back(&t);
                    //for(auto & [n,l] : t.children){
                    //    next_layer.push_back(&l);
                    //}
                }
            }
        }

        current_layer = next_layer;
        appender = next_appender;
        next_appender = {};
        next_layer = {};
    }
    return true;
}