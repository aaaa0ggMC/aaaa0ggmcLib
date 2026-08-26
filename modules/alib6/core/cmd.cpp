/**
 * @file cmd.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 现代化命令行 CLI 解释与分发引擎实现
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6 {

    Command::Command(memory_resource* mem)
        : parser(mem)
        , router(mem)
        , registered_options(mem)
        , registered_toggles(mem)
        , alias_to_opt_idx(mem)
        , alias_to_toggle_idx(mem)
        , dispatchers(mem) {}

    void Command::register_option(Option opt) {
        usize idx = registered_options.size();
        registered_options.push_back(opt);

        auto mem = alias_to_opt_idx.get_allocator().resource();
        if (!opt.name.empty()) alias_to_opt_idx.emplace(pmr::string(opt.name, mem), idx);
        if (!opt.short_name.empty()) alias_to_opt_idx.emplace(pmr::string(opt.short_name, mem), idx);
        if (!opt.long_name.empty()) alias_to_opt_idx.emplace(pmr::string(opt.long_name, mem), idx);
    }

    void Command::register_toggle(Toggle tog) {
        usize idx = registered_toggles.size();
        registered_toggles.push_back(tog);

        auto mem = alias_to_toggle_idx.get_allocator().resource();
        if (!tog.name.empty()) alias_to_toggle_idx.emplace(pmr::string(tog.name, mem), idx);
        if (!tog.short_name.empty()) alias_to_toggle_idx.emplace(pmr::string(tog.short_name, mem), idx);
        if (!tog.long_name.empty()) alias_to_toggle_idx.emplace(pmr::string(tog.long_name, mem), idx);
    }

    bool Command::CommandInput::has(std::string_view name) const noexcept {
        auto it = cmd.alias_to_toggle_idx.find(name);
        if (it != cmd.alias_to_toggle_idx.end()) {
            const auto& tog = cmd.registered_toggles[it->second];
            for (const auto& layer : found_toggles) {
                if ((!tog.name.empty() && layer.contains(tog.name)) ||
                    (!tog.short_name.empty() && layer.contains(tog.short_name)) ||
                    (!tog.long_name.empty() && layer.contains(tog.long_name))) {
                    return true;
                }
            }
        }

        // 兼容在 toggles 中直接按原始字符串查找
        for (const auto& layer : found_toggles) {
            if (layer.contains(name)) return true;
        }
        return false;
    }

    pvalue_t Command::CommandInput::get(std::string_view name) const noexcept {
        auto it = cmd.alias_to_opt_idx.find(name);
        if (it != cmd.alias_to_opt_idx.end()) {
            const auto& opt = cmd.registered_options[it->second];
            // 从当前层级向上回溯搜索
            for (auto layer_it = found_options.rbegin(); layer_it != found_options.rend(); ++layer_it) {
                if (!opt.long_name.empty()) {
                    auto match = layer_it->find(opt.long_name);
                    if (match != layer_it->end()) return match->second;
                }
                if (!opt.short_name.empty()) {
                    auto match = layer_it->find(opt.short_name);
                    if (match != layer_it->end()) return match->second;
                }
                if (!opt.name.empty()) {
                    auto match = layer_it->find(opt.name);
                    if (match != layer_it->end()) return match->second;
                }
            }

            if (!opt.default_val.empty()) {
                return pvalue_t(opt.default_val);
            }
        }

        for (auto layer_it = found_options.rbegin(); layer_it != found_options.rend(); ++layer_it) {
            auto match = layer_it->find(name);
            if (match != layer_it->end()) return match->second;
        }

        return pvalue_t(false);
    }

    panalyser_t Command::judge_fn(
        pcursor_t* cursor,
        pmr::unordered_set<std::string_view>& found_t,
        pmr::unordered_map<std::string_view, pvalue_t>& found_o
    ) {
        pvalue_t nval;
        while (*cursor && (nval = cursor->peek())) {
            bool matched = false;

            // 1. 尝试匹配 Option (如 --port=8080, --port 8080, -p 8080)
            for (const auto& opt : registered_options) {
                std::array<std::string_view, 3> keys = {opt.long_name, opt.short_name, opt.name};
                for (auto k : keys) {
                    if (k.empty()) continue;
                    if (nval.data.starts_with(k)) {
                        if (nval.data.size() > k.size() && nval.data[k.size()] != '=') {
                            continue;
                        }
                        cursor->next();
                        auto bundle = cursor->next_value_bundle("=");
                        found_o.emplace(k, bundle.second);
                        matched = true;
                        break;
                    }
                }
                if (matched) break;
            }
            if (matched) continue;

            // 2. 尝试匹配 Toggle Flag (如 --verbose, -v)
            for (const auto& tog : registered_toggles) {
                std::array<std::string_view, 3> keys = {tog.long_name, tog.short_name, tog.name};
                for (auto k : keys) {
                    if (!k.empty() && nval.data == k) {
                        found_t.emplace(k);
                        cursor->next();
                        matched = true;
                        break;
                    }
                }
                if (matched) break;
            }
            if (matched) continue;

            break;
        }

        if (*cursor) return cursor->commit();
        return cursor->invalid();
    }

    pmr::vector<Command::CommandOutput> Command::dispatch_pipeline(bool remove_head) {
        auto mem = parser.resource;
        pmr::vector<pmr::unordered_set<std::string_view>> find_toggles(mem);
        pmr::vector<pmr::unordered_map<std::string_view, pvalue_t>> find_options(mem);
        pmr::vector<CommandOutput> outputs(mem);

        Router::DispatchResult result = router.match<
            Router::MatchSettings{.analyser = false}
        >(
            parser,
            [&](pcursor_t* p) {
                return judge_fn(
                    p,
                    find_toggles.emplace_back(),
                    find_options.emplace_back()
                );
            },
            [&](panalyser_t* ana) {
                pmr::unordered_set<std::string_view>* last_t = nullptr;
                pmr::unordered_map<std::string_view, pvalue_t>* last_o = nullptr;

                if (!find_toggles.empty()) last_t = &find_toggles.back();
                else last_t = &find_toggles.emplace_back();

                if (!find_options.empty()) last_o = &find_options.back();
                else last_o = &find_options.emplace_back();

                for (const auto& opt : registered_options) {
                    std::array<std::string_view, 3> keys = {opt.long_name, opt.short_name, opt.name};
                    for (auto k : keys) {
                        if (k.empty()) continue;
                        auto vec = ana->extract_options(k, "=");
                        if (!vec.empty()) {
                            last_o->emplace(k, vec.back());
                            break;
                        }
                    }
                }

                for (const auto& tog : registered_toggles) {
                    std::array<std::string_view, 3> keys = {tog.long_name, tog.short_name, tog.name};
                    for (auto k : keys) {
                        if (!k.empty() && ana->extract_flag(k)) {
                            last_t->emplace(k);
                            break;
                        }
                    }
                }
            },
            remove_head
        );

        if (result.dispatches.empty()) {
            result.dispatches.push_back(rdispatcher_t(0));
        }

        for (usize i = 0; i < result.dispatches.size(); ++i) {
            std::function<CommandOutput(const CommandInput&)>* func = nullptr;
            i64 disp_id = result.dispatches[i].id;

            if (disp_id != 0) {
                auto it = dispatchers.find(disp_id);
                if (it != dispatchers.end()) func = &it->second;
            } else if (default_dispatcher) {
                func = &default_dispatcher;
            }

            if (func) {
                CommandInput in{
                    (i + 1 == result.dispatches.size()),
                    i,
                    find_toggles,
                    find_options,
                    result.keys,
                    result.remains,
                    result.routes,
                    *this
                };
                CommandOutput co = (*func)(in);
                if (co) {
                    outputs.emplace_back(co);
                }
                if (co.should_terminate) break;
            }
        }

        if (outputs.empty()) {
            outputs.emplace_back(CommandOutput::no_output());
        }
        return outputs;
    }

    pmr::string Command::generate_help_string(memory_resource* mem) const {
        pmr::string buf(mem);

        constexpr std::string_view STEP = "    ";
        constexpr std::string_view BAR  = "│   ";
        constexpr std::string_view TEE  = "├── ";
        constexpr std::string_view ELB  = "└── ";

        // 1. 递归输出路由树 (DFS)
        struct NodeEntry {
            const RouterNode* node{nullptr};
            std::string_view name;
        };

        std::vector<NodeEntry> stack;
        for (const auto& [k, v] : router.root.children) {
            stack.push_back(NodeEntry{&v, k});
        }
        std::reverse(stack.begin(), stack.end());

        alib6::storage::MonoBitSet bits(64, mem);
        int depth = 0;

        while (!stack.empty()) {
            auto current = stack.back();
            stack.pop_back();

            if (current.node == nullptr) {
                --depth;
                continue;
            }

            // 输出前缀缩进
            for (int i = 0; i < depth - 1; ++i) {
                buf.append(bits.get(i + 1) ? STEP : BAR);
            }

            bits.ensure(depth + 1);
            if (depth >= 1) {
                if (stack.empty() || stack.back().node == nullptr) {
                    buf.append(ELB);
                    bits.set(depth);
                } else {
                    buf.append(TEE);
                    bits.reset(depth);
                }
            }

            // 输出节点名称
            if (current.node->rule == RouterNode::MatchRule::Any) {
                std::format_to(std::back_inserter(buf), "{{{}}}\n", current.name);
            } else {
                std::format_to(std::back_inserter(buf), "{}\n", current.name);
            }

            if (!current.node->children.empty()) {
                stack.push_back(NodeEntry{nullptr, ""});
                usize start_idx = stack.size();
                for (const auto& [k, v] : current.node->children) {
                    stack.push_back(NodeEntry{&v, k});
                }
                std::reverse(stack.begin() + start_idx, stack.end());
                ++depth;
            }
        }

        // 2. 输出支持的选项列表
        if (!registered_options.empty()) {
            buf.append("\nSupported options:\n");
            usize max_len = 0;
            for (const auto& opt : registered_options) {
                usize len = opt.name.size();
                if (!opt.short_name.empty()) len += opt.short_name.size() + 2;
                if (!opt.long_name.empty()) len += opt.long_name.size() + 2;
                if (len > max_len) max_len = len;
            }
            max_len += 2;

            for (const auto& opt : registered_options) {
                std::string names;
                if (!opt.short_name.empty()) {
                    names += opt.short_name;
                    names += ", ";
                }
                if (!opt.long_name.empty()) {
                    names += opt.long_name;
                    if (names.size() > opt.long_name.size() + 2) names += " ";
                }
                if (names.empty()) names = opt.name;

                std::string def_str;
                if (!opt.default_val.empty()) {
                    def_str = std::format("[default: {}]", opt.default_val);
                }

                std::format_to(
                    std::back_inserter(buf),
                    "  {:<{}} {:<16} {}\n",
                    names, max_len,
                    def_str,
                    opt.description
                );
            }
        }

        // 3. 输出支持的开关列表
        if (!registered_toggles.empty()) {
            buf.append("\nSupported toggles:\n");
            usize max_len = 0;
            for (const auto& tog : registered_toggles) {
                usize len = tog.name.size();
                if (!tog.short_name.empty()) len += tog.short_name.size() + 2;
                if (!tog.long_name.empty()) len += tog.long_name.size() + 2;
                if (len > max_len) max_len = len;
            }
            max_len += 2;

            for (const auto& tog : registered_toggles) {
                std::string names;
                if (!tog.short_name.empty()) {
                    names += tog.short_name;
                    names += ", ";
                }
                if (!tog.long_name.empty()) {
                    names += tog.long_name;
                }
                if (names.empty()) names = tog.name;

                std::format_to(
                    std::back_inserter(buf),
                    "  {:<{}} {}\n",
                    names, max_len,
                    tog.description
                );
            }
        }

        return buf;
    }

} // namespace alib6
