/**
 * @file flat.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Flat 扁平化文本策略的转储与有损解析实现 (alib6.data)
 * @version 6.0
 * @date 2026-09-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <charconv>
#include <string_view>

module alib6.data;
import std;
import alib6.core;

namespace pmr = std::pmr;

namespace alib6::data {

namespace {

    struct DumpContext {
        Flat::__dump_fn* fn{nullptr};
        void* p{nullptr};
        const FlatConfig* cfg{nullptr};
        bool first{true};
    };

    struct PathStep {
        bool is_index{false};
        std::string_view key{};
        usize index{0};
    };

    /// 判断字符串是否会被解析端识别为数字 (如是, 转储时需加引号以避免类型歧义)
    bool looks_like_number(std::string_view s) {
        if (s.empty()) return false;
        i64 iv{};
        auto ri = std::from_chars(s.data(), s.data() + s.size(), iv);
        if (ri.ec == std::errc() && ri.ptr == s.data() + s.size()) return true;
        double dv{};
        auto rd = std::from_chars(s.data(), s.data() + s.size(), dv);
        return rd.ec == std::errc() && rd.ptr == s.data() + s.size();
    }

    /// 判断字符串值是否需要加引号包裹
    bool needs_quote(std::string_view s, const FlatConfig& cfg) {
        if (s.empty()) return true;
        if (s == cfg.null_text || s == "true" || s == "false") return true;
        if (looks_like_number(s)) return true;
        for (char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (u <= 0x20 || u == 0x7f) return true;
            switch (c) {
                case '"': case '\\': case '{': case '}':
                case '[': case ']':
                    return true;
                default: break;
            }
        }
        if (!cfg.item_sep.empty() && s.find(cfg.item_sep) != std::string_view::npos) return true;
        if (!cfg.key_value_sep.empty() && s.find(cfg.key_value_sep) != std::string_view::npos) return true;
        return false;
    }

    void append_number(pmr::string& out, usize value) {
        char buf[24];
        auto res = std::to_chars(buf, buf + sizeof(buf), value);
        out.append(buf, res.ptr);
    }

    void emit_scalar(DumpContext& ctx, const AData& node, memory_resource* mem) {
        const auto& cfg = *ctx.cfg;

        if (node.is_null()) {
            (*ctx.fn)(cfg.null_text, ctx.p);
            return;
        }

        const auto& v = node.value();
        if (v.get_type() == Value::STRING) {
            auto raw = v.to<std::string_view>();
            const bool quote = cfg.always_quote_string || (cfg.quote_when_needed && needs_quote(raw, cfg));
            if (quote) {
                auto escaped = alib6::str::escape(raw, false, mem);
                (*ctx.fn)("\"", ctx.p);
                (*ctx.fn)(escaped, ctx.p);
                (*ctx.fn)("\"", ctx.p);
            } else {
                (*ctx.fn)(raw, ctx.p);
            }
            return;
        }

        if (v.get_type() == Value::FLOATING && cfg.float_precision >= 0) {
            pmr::string buf(mem);
            std::format_to(std::back_inserter(buf), "{:.{}f}", v.to<double>(), cfg.float_precision);
            (*ctx.fn)(buf, ctx.p);
            return;
        }

        (*ctx.fn)(v.to<std::string>(), ctx.p);
    }

    void dump_node(DumpContext& ctx, const AData& node, pmr::string& path, memory_resource* mem) {
        const auto& cfg = *ctx.cfg;

        if (node.is_object()) {
            const auto& obj = node.object();
            pmr::vector<std::string_view> keys(mem);
            keys.reserve(obj.size());
            for (auto proxy : obj) {
                keys.push_back(proxy.first());
            }
            if (cfg.sort_keys) {
                std::sort(keys.begin(), keys.end());
            }
            for (auto key : keys) {
                const auto* child = obj.at_ptr(key);
                if (!child) continue;
                const auto old = path.size();
                if (!path.empty()) path += cfg.path_sep;
                path += key;
                dump_node(ctx, *child, path, mem);
                path.resize(old);
            }
            return;
        }

        if (node.is_array()) {
            const auto& arr = node.array();
            for (usize i = 0; i < arr.size(); ++i) {
                const auto old = path.size();
                if (cfg.array_bracket) {
                    path += cfg.array_open;
                    append_number(path, i);
                    path += cfg.array_close;
                } else {
                    if (!path.empty()) path += cfg.path_sep;
                    append_number(path, i);
                }
                dump_node(ctx, arr[static_cast<ptrdiff_t>(i)], path, mem);
                path.resize(old);
            }
            return;
        }

        if (!ctx.first) (*ctx.fn)(cfg.item_sep, ctx.p);
        ctx.first = false;
        if (!path.empty()) {
            (*ctx.fn)(path, ctx.p);
            (*ctx.fn)(cfg.key_value_sep, ctx.p);
        }
        emit_scalar(ctx, node, mem);
    }

    /// 在引号之外查找分隔符首次出现的位置
    usize find_unquoted(std::string_view s, std::string_view sep) {
        if (sep.empty()) return std::string_view::npos;
        bool in_quotes = false;
        usize i = 0;
        while (i + sep.size() <= s.size()) {
            const char c = s[i];
            if (in_quotes && c == '\\') { i += 2; continue; }
            if (c == '"') { in_quotes = !in_quotes; ++i; continue; }
            if (!in_quotes && s.substr(i).starts_with(sep)) return i;
            ++i;
        }
        return std::string_view::npos;
    }

    /// 按 item_sep 切分条目 (引号内的分隔符不切分), 忽略空白条目
    void tokenize(std::string_view input, const FlatConfig& cfg, pmr::vector<std::string_view>& out) {
        if (cfg.item_sep.empty()) {
            out.push_back(input);
            return;
        }

        usize start = 0;
        bool in_quotes = false;
        usize i = 0;
        while (i < input.size()) {
            const char c = input[i];
            if (in_quotes && c == '\\' && i + 1 < input.size()) { i += 2; continue; }
            if (c == '"') { in_quotes = !in_quotes; ++i; continue; }
            if (!in_quotes && input.substr(i).starts_with(cfg.item_sep)) {
                auto tok = alib6::str::trim(input.substr(start, i - start));
                if (!tok.empty()) out.push_back(tok);
                i += cfg.item_sep.size();
                start = i;
                continue;
            }
            ++i;
        }
        auto tok = alib6::str::trim(input.substr(start));
        if (!tok.empty()) out.push_back(tok);
    }

    /// 将有损文本值写入目标节点
    bool assign_value(AData& node, std::string_view raw, const FlatConfig& cfg, memory_resource* mem) {
        if (!node.is_null() && !node.is_value()) return false;

        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
            auto un = alib6::str::unescape(raw.substr(1, raw.size() - 2), mem);
            node = std::string_view(un);
            return true;
        }
        if (raw == cfg.null_text) {
            node.set_null();
            return true;
        }
        if (raw == "true") { node = true; return true; }
        if (raw == "false") { node = false; return true; }

        i64 iv{};
        auto ri = std::from_chars(raw.data(), raw.data() + raw.size(), iv);
        if (ri.ec == std::errc() && ri.ptr == raw.data() + raw.size()) {
            node = iv;
            return true;
        }

        double dv{};
        auto rd = std::from_chars(raw.data(), raw.data() + raw.size(), dv);
        if (rd.ec == std::errc() && rd.ptr == raw.data() + raw.size()) {
            node = dv;
            return true;
        }

        node = raw;
        return true;
    }

    /// 将路径 (如 a.b[0]) 解析为步骤序列
    bool parse_path(std::string_view path, const FlatConfig& cfg, pmr::vector<PathStep>& out) {
        if (path.empty()) return false;
        if (cfg.path_sep.empty()) return false;
        if (cfg.array_bracket && (cfg.array_open.empty() || cfg.array_close.empty())) return false;

        usize i = 0;
        while (i < path.size()) {
            if (cfg.array_bracket && path.substr(i).starts_with(cfg.array_open)) {
                i += cfg.array_open.size();
                usize idx = 0;
                bool any = false;
                while (i < path.size() && path[i] >= '0' && path[i] <= '9') {
                    idx = idx * 10 + static_cast<usize>(path[i] - '0');
                    ++i;
                    any = true;
                }
                if (!any) return false;
                if (!path.substr(i).starts_with(cfg.array_close)) return false;
                i += cfg.array_close.size();
                out.push_back(PathStep{.is_index = true, .key = {}, .index = idx});
                if (i >= path.size()) break;
                if (!path.substr(i).starts_with(cfg.path_sep)) return false;
                i += cfg.path_sep.size();
                if (i >= path.size()) return false;
                continue;
            }

            const usize start = i;
            while (i < path.size() && !path.substr(i).starts_with(cfg.path_sep) &&
                   !(cfg.array_bracket && path.substr(i).starts_with(cfg.array_open))) {
                ++i;
            }
            auto key = path.substr(start, i - start);
            if (key.empty()) return false;

            bool as_index = false;
            usize idx_val = 0;
            if (!cfg.array_bracket) {
                bool all_digits = true;
                for (char c : key) {
                    if (c < '0' || c > '9') { all_digits = false; break; }
                }
                if (all_digits) {
                    as_index = true;
                    for (char c : key) idx_val = idx_val * 10 + static_cast<usize>(c - '0');
                }
            }

            if (as_index) {
                out.push_back(PathStep{.is_index = true, .key = {}, .index = idx_val});
            } else {
                out.push_back(PathStep{.is_index = false, .key = key, .index = 0});
            }

            if (i < path.size()) {
                if (path.substr(i).starts_with(cfg.path_sep)) {
                    i += cfg.path_sep.size();
                    if (i >= path.size()) return false;
                } else if (!(cfg.array_bracket && path.substr(i).starts_with(cfg.array_open))) {
                    return false;
                }
            }
        }
        return !out.empty();
    }

    /// 沿路径逐层创建容器并写入值
    bool resolve_and_set(AData& root, std::string_view path, std::string_view raw,
                         const FlatConfig& cfg, memory_resource* mem) {
        pmr::vector<PathStep> steps(mem);
        if (!parse_path(path, cfg, steps)) return false;

        AData* node = &root;
        for (const auto& st : steps) {
            if (st.is_index) {
                if (node->is_null()) node->set<AData::Array>();
                else if (!node->is_array()) return false;
                node = &(*node)[static_cast<ptrdiff_t>(st.index)];
            } else {
                if (node->is_null()) node->set<AData::Object>();
                else if (!node->is_object()) return false;
                node = &(*node)[st.key];
            }
        }
        return assign_value(*node, raw, cfg, mem);
    }

} // namespace

    void Flat::__internal_dump(__dump_fn fn, void* p, const AData& root) const {
        auto* mem = root.get_allocator();
        DumpContext ctx{fn, p, &cfg, true};
        pmr::string path(mem);
        dump_node(ctx, root, path, mem);
    }

    bool Flat::parse(std::string_view data, AData& root) {
        auto* mem = root.get_allocator();
        root.set_null();

        const auto input = alib6::str::trim(data);
        if (input.empty()) return true;
        if (cfg.key_value_sep.empty()) return false;

        pmr::vector<std::string_view> tokens(mem);
        tokenize(input, cfg, tokens);
        if (tokens.empty()) return true;

        // 根标量: 仅一个不含未引用键值分隔符的条目
        if (tokens.size() == 1 && find_unquoted(tokens[0], cfg.key_value_sep) == std::string_view::npos) {
            return assign_value(root, tokens[0], cfg, mem);
        }

        for (auto token : tokens) {
            const auto pos = find_unquoted(token, cfg.key_value_sep);
            if (pos == std::string_view::npos) return false;

            auto key = alib6::str::trim(token.substr(0, pos));
            auto raw = alib6::str::trim(token.substr(pos + cfg.key_value_sep.size()));
            if (key.empty()) return false;

            if (!resolve_and_set(root, key, raw, cfg, mem)) return false;
        }
        return true;
    }

} // namespace alib6::data
