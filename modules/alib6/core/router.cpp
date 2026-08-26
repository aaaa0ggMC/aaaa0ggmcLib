/**
 * @file router.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 路由节点构建与前缀树路由合并实现
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <deque>

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6 {

    Router::Node Router::Node::Group(std::string_view name, std::optional<RouterNode::Dispatcher> dispatch) {
        if (!parent) return {nullptr};
        if (name.empty()) return {parent};

        auto rulep = RouterNode::MatchRule::Full;
        RouterNode* n = nullptr;
        if (name.size() >= 2 && name.front() == '{' && name.back() == '}') {
            name = name.substr(1, name.size() - 2);
            rulep = RouterNode::MatchRule::Any;
        }

        pmr::string pname(str::unescape(name, parent->children.get_allocator().resource()), parent->children.get_allocator().resource());
        auto it = parent->children.find(pname);
        if (it == parent->children.end()) {
            n = &parent->children.emplace(pname, RouterNode{parent->children.get_allocator().resource()}).first->second;
            n->rule = rulep;
        } else {
            n = &it->second;
        }
        if (dispatch) n->dispatcher = *dispatch;
        return {n};
    }

    bool Router::add_route(std::span<const std::string_view> path, RouterNode::Dispatcher dispatcher, ErrorWrapper err) noexcept {
        if (path.empty()) return false;
        RouterNode root_temp(root.children.get_allocator().resource());
        auto name = path[0];
        if (name.empty()) return false;

        if (name.size() >= 2 && name.front() == '{' && name.back() == '}') {
            name = name.substr(1, name.size() - 2);
            root_temp.rule = RouterNode::MatchRule::Any;
        } else {
            root_temp.rule = RouterNode::MatchRule::Follow;
        }
        RouterNode* focus = &root_temp;

        for (auto d : path.subspan(1)) {
            if (d.empty()) continue;
            auto rule = RouterNode::MatchRule::Follow;

            if (d.size() >= 2 && d.front() == '{' && d.back() == '}') {
                d = d.substr(1, d.size() - 2);
                rule = RouterNode::MatchRule::Any;
            }

            focus = &focus->children.emplace(
                str::unescape(d, root.children.get_allocator().resource()),
                RouterNode{root.children.get_allocator().resource()}
            ).first->second;

            focus->rule = rule;
        }

        focus->dispatcher = dispatcher;
        return add_route(str::unescape(name, root.children.get_allocator().resource()), root_temp, err);
    }

    bool Router::add_route(std::string_view node_name, const RouterNode& nodes, ErrorWrapper err) noexcept {
        std::deque<const RouterNode*> current_layer;
        std::deque<const RouterNode*> next_layer;

        std::deque<RouterNode*> appender;
        std::deque<RouterNode*> next_appender;
        {
            RouterNode* current_focus = nullptr;
            for (auto& [name, node] : root.children) {
                if (name == node_name) {
                    current_focus = &node;
                    if (nodes.rule != RouterNode::MatchRule::Follow && nodes.rule != node.rule) {
                        err.report("Router: Conflict root route rule for '{}'", node_name);
                        return false;
                    }
                    if (nodes.rule != RouterNode::MatchRule::Follow) {
                        current_focus->dispatcher = nodes.dispatcher;
                    }
                    break;
                }
            }
            if (!current_focus) {
                current_focus = &(root.children.emplace(
                    node_name,
                    nodes
                ).first->second);
            }
            appender.push_back(current_focus);
        }

        current_layer.push_back(nullptr);
        current_layer.push_back(&nodes);

        while (!current_layer.empty()) {
            RouterNode* current = nullptr;
            while (!current_layer.empty()) {
                auto val = current_layer.front();
                current_layer.pop_front();
                if (val == nullptr) {
                    if (!appender.empty()) {
                        current = appender.front();
                        appender.pop_front();
                    }
                    continue;
                }

                if (!current) continue;
                bool has_any = current->find_any().second != nullptr;

                for (auto& [name, t] : val->children) {
                    auto it = current->children.find(name);
                    if (it == current->children.end()) {
                        if (t.rule == RouterNode::MatchRule::Any && has_any) {
                            err.report("Router: A node cannot have 2 ANY leaves! ('{}')", name);
                            return false;
                        }
                        current->children.emplace(name, t);
                    } else {
                        RouterNode& r = it->second;
                        if (t.rule != RouterNode::MatchRule::Follow && r.rule != t.rule) {
                            err.report("Router: Conflict route rule for '{}'", name);
                            return false;
                        }
                        if (t.rule != RouterNode::MatchRule::Follow) {
                            r.dispatcher = t.dispatcher;
                        }
                        next_appender.push_back(&r);
                        next_layer.push_back(nullptr);
                        next_layer.push_back(&t);
                    }
                }
            }

            current_layer = std::move(next_layer);
            appender = std::move(next_appender);
            next_appender.clear();
            next_layer.clear();
        }
        return true;
    }

} // namespace alib6
