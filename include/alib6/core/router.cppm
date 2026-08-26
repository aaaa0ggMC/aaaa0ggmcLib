/**
 * @file router.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 基于前缀树的命令/路径分发路由器 (支持 Any 动态路径参数提取)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <deque>

export module alib6.core:router;
import std;
import :types;
import :memory;
import :concepts;
import :error;
import :debug;
import :bstd;
import :str;
import :parser;

namespace pmr = std::pmr;

export namespace alib6 {

    template<class T>
    concept JudgeFn = requires(T&& t, pcursor_t& cursor) {
        { t(&cursor) } -> std::convertible_to<panalyser_t>;
    };

    template<class T>
    concept RoamerFn = requires(T&& t, panalyser_t& cursor) {
        { t(&cursor) } -> std::convertible_to<void>;
    };

    /**
     * @brief 路由树节点
     */
    struct RouterNode {
        struct Dispatcher {
            i64 id{0};
            constexpr Dispatcher(i64 i = 0) noexcept : id(i) {}
            constexpr operator i64() const noexcept { return id; }

            template<class Target>
            void write_to_log(Target& target) const {
                std::format_to(std::back_inserter(target), "Dispatcher({})", id);
            }
        };

        enum class MatchRule : u8 {
            Follow,
            Full,
            Any
        };

        MatchRule rule{MatchRule::Follow};
        Dispatcher dispatcher{0};
        pmr::unordered_map<
            pmr::string,
            RouterNode,
            TransparentStringHash,
            TransparentStringEqual
        > children;

    private:
        mutable RouterNode* any_cache{nullptr};
        mutable std::string_view any_name{""};

    public:
        explicit RouterNode(memory_resource* mem = get_default_resource())
            : children(mem) {}

        std::pair<std::string_view, RouterNode*> find_any() const {
            if (any_cache) {
                return {any_name, any_cache};
            }
            for (auto& [name, v] : children) {
                if (v.rule == MatchRule::Any) {
                    any_cache = const_cast<RouterNode*>(&v);
                    any_name = name;
                    return {any_name, any_cache};
                }
            }
            return {"", nullptr};
        }
    };

    using rdispatcher_t = RouterNode::Dispatcher;

    /**
     * @brief 路径与命令分发路由器
     */
    struct Router {
        RouterNode root;

        explicit Router(memory_resource* mem = get_default_resource())
            : root(mem) {}

        struct DispatchResult {
            pmr::vector<RouterNode::Dispatcher> dispatches;
            pmr::unordered_map<
                pmr::string,
                pmr::string,
                TransparentStringHash,
                TransparentStringEqual
            > keys;
            pmr::vector<panalyser_t> depth_analysers;
            pmr::vector<std::string_view> remains;
            pmr::vector<std::string_view> routes;

            explicit DispatchResult(memory_resource* mem = get_default_resource())
                : dispatches(mem), keys(mem), depth_analysers(mem), remains(mem), routes(mem) {}

            template<class Target>
            void write_to_log(Target& target) const {
                std::format_to(
                    std::back_inserter(target),
                    "DispatchResult(dispatches={}, keys={}, remains={})",
                    dispatches.size(), keys.size(), remains.size()
                );
            }
        };

        struct MatchSettings {
            bool keys{true};
            bool analyser{true};
            bool remains{true};
            bool dispatchers{true};
            bool routes{true};
        };

        constexpr static MatchSettings default_match_settings{true, true, true, true, true};

        struct Node {
            RouterNode* parent{nullptr};

            Node Group(std::string_view name, std::optional<RouterNode::Dispatcher> dispatch = std::nullopt);

            void dispatch(RouterNode::Dispatcher disp) {
                if (!parent) panic("Invalid group context");
                parent->dispatcher = disp;
            }

            void rule(RouterNode::MatchRule r) {
                if (!parent) panic("Invalid group context");
                parent->rule = r;
            }

            void operator=(RouterNode::Dispatcher disp) {
                dispatch(disp);
            }

            template<class T>
            void operator=(T&& v) requires requires(T& t) { t.dispatch(*this); } {
                v.dispatch(*this);
            }
        };

        void clear() {
            root = RouterNode{root.children.get_allocator().resource()};
        }

        Node Group(std::string_view name, std::optional<RouterNode::Dispatcher> dispatch = std::nullopt) {
            return Node{&root}.Group(name, dispatch);
        }

        bool add_route(std::string_view node_name, const RouterNode& nodes, ErrorWrapper err = {}) noexcept;
        bool add_route(std::span<const std::string_view> path, RouterNode::Dispatcher dispatcher, ErrorWrapper err = {}) noexcept;

        bool add_route(std::string_view path, RouterNode::Dispatcher dispatcher, ErrorWrapper err = {}) {
            auto segments = str::split(path, '/', root.children.get_allocator().resource());
            usize valid = 0;
            for (auto v : segments) {
                if (v.empty()) ++valid;
                else break;
            }
            if (valid == segments.size()) return false;
            return add_route(std::span<const std::string_view>(segments.data() + valid, segments.size() - valid), dispatcher, err);
        }

        template<
            MatchSettings settings = default_match_settings,
            JudgeFn Judge,
            RoamerFn Roamer
        >
        DispatchResult match(panalyser_t parser, Judge&& fn, Roamer&& rn, bool remove_head = true) noexcept {
            DispatchResult result(root.children.get_allocator().resource());
            if (remove_head && !parser.inputs.empty()) {
                parser.inputs.erase(parser.inputs.begin());
            }

            pcursor_t c = parser.as_cursor();
            RouterNode* focus = &root;

            while (parser && c && !focus->children.empty()) {
                auto token = c.head();
                auto it = focus->children.find(token.data);
                if (it != focus->children.end() && it->second.rule != RouterNode::MatchRule::Any) {
                    focus = &(it->second);
                    if constexpr (settings.routes) {
                        result.routes.emplace_back(it->first);
                    }
                } else {
                    auto [any_k, any_node] = focus->find_any();
                    if (any_node) {
                        focus = any_node;
                        if constexpr (settings.keys) {
                            result.keys.emplace(any_k, token.data);
                        }
                        if constexpr (settings.routes) {
                            result.routes.emplace_back(any_k);
                        }
                    } else {
                        break;
                    }
                }

                if constexpr (settings.dispatchers) {
                    result.dispatches.push_back(focus->dispatcher);
                }
                if constexpr (settings.analyser) {
                    result.depth_analysers.push_back(fn(&c));
                } else {
                    fn(&c);
                }

                if (c) c.commit();
                c.~Cursor();
                new (&c) pcursor_t(parser.as_cursor());
            }

            bool need_build = true;
            if (parser.inputs.size() == 1) {
                auto it = focus->children.find(parser.inputs[0]);
                RouterNode::Dispatcher disp;

                if (it != focus->children.end()) {
                    need_build = false;
                    disp = it->second.dispatcher;
                    if constexpr (settings.routes) {
                        result.routes.emplace_back(it->first);
                    }
                } else {
                    auto [any_k, any_node] = focus->find_any();
                    if (any_node) {
                        if constexpr (settings.keys) {
                            result.keys.emplace(any_k, parser.inputs[0]);
                        }
                        disp = any_node->dispatcher;
                        need_build = false;
                        if constexpr (settings.routes) {
                            result.routes.emplace_back(any_k);
                        }
                    }
                }

                if (!need_build) {
                    auto cc = parser.as_cursor();
                    if constexpr (settings.dispatchers) {
                        result.dispatches.push_back(disp);
                    }
                    if constexpr (settings.analyser) {
                        result.depth_analysers.emplace_back(fn(&cc));
                    } else {
                        fn(&cc);
                    }
                    if constexpr (settings.remains) {
                        result.remains = {};
                    }
                }
            }

            if constexpr (settings.remains) {
                if (need_build && !parser.inputs.empty()) {
                    parser.inputs.insert(parser.inputs.begin(), "--REMAINS--");
                    rn(&parser);
                    if (parser.inputs.size() > 1) {
                        result.remains.insert(result.remains.begin(), parser.inputs.begin() + 1, parser.inputs.end());
                    }
                }
            }
            return result;
        }

        template<
            MatchSettings settings = default_match_settings,
            JudgeFn Judge,
            RoamerFn Roamer
        >
        DispatchResult match(Parser& parser, Judge&& fn, Roamer&& rn, bool remove_head = true) noexcept {
            return match<settings>(parser.analyse(), std::forward<Judge>(fn), std::forward<Roamer>(rn), remove_head);
        }
    };

    using rgroup_t = Router::Node;

} // namespace alib6
