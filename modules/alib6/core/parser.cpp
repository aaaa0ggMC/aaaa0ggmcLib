/**
 * @file parser.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 命令行词法分词器与事务游标实现
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cctype>

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6 {

    Parser::Parser(memory_resource* mem)
        : resource(mem), head(mem), arg_full(mem), args(mem) {}

    Parser::Analyser::Analyser(Parser& p)
        : parser(p), inputs(p.args.begin(), p.args.end(), p.resource) {}

    Parser::Analyser::Analyser(Parser& p, std::span<std::string_view> slice)
        : parser(p), inputs(slice.begin(), slice.end(), p.resource) {}

    Parser::Analyser::Cursor::Cursor(Analyser& ana, std::span<std::string_view> d, usize pi, bool mt, usize beg)
        : m_analyser(ana), data(d), parent_index(pi), cursor(beg), matched(mt) {}

    Parser::Analyser::Cursor Parser::Analyser::Cursor::operator||(const Cursor& c) const noexcept {
        if (c.matched && !matched) {
            return c;
        }
        return *this;
    }

    bool Parser::Analyser::Cursor::match(std::string_view str) noexcept {
        auto p = peek();
        return p && p.data == str;
    }

    bool Parser::Analyser::Cursor::match(std::span<const std::string_view> strs) noexcept {
        for (auto c : strs) {
            if (match(c)) return true;
        }
        return false;
    }

    Parser::Analyser Parser::Analyser::Cursor::invalid() noexcept {
        return Analyser(m_analyser.parser, std::span<std::string_view>{});
    }

    Parser::Value Parser::Analyser::Cursor::peek() const noexcept {
        if (!matched) return Value(false);
        usize cur = this->cursor;
        if (!processed && !prefix.empty()) {
            if (cur > 0 && cur - 1 < data.size()) {
                auto s = data[cur - 1];
                if (s.starts_with(prefix)) {
                    usize sz = prefix.size();
                    if (!opt_str.empty()) {
                        if (s.substr(prefix.size()).starts_with(opt_str)) {
                            sz += opt_str.size();
                        } else if (cur < data.size() && data[cur].starts_with(opt_str)) {
                            s = data[cur++];
                            sz = opt_str.size();
                        }
                    }
                    s = str::trim(s.substr(sz));
                    if (!s.empty()) return Value(s);
                }
            }
        }
        if (cur >= data.size()) return Value(false);
        return Value(data[cur]);
    }

    Parser::Value Parser::Analyser::Cursor::next() noexcept {
        if (!matched) return Value(false);
        if (!processed && !prefix.empty()) {
            processed = true;
            if (cursor > 0 && cursor - 1 < data.size()) {
                auto s = data[cursor - 1];
                if (s.starts_with(prefix)) {
                    usize sz = prefix.size();
                    if (!opt_str.empty()) {
                        if (s.substr(prefix.size()).starts_with(opt_str)) {
                            sz += opt_str.size();
                        } else if (cursor < data.size() && data[cursor].starts_with(opt_str)) {
                            s = data[cursor++];
                            sz = opt_str.size();
                        }
                    }
                    s = str::trim(s.substr(sz));
                    if (!s.empty()) return Value(s);
                }
            }
        }
        if (cursor >= data.size()) return Value(false);
        return Value(data[cursor++]);
    }

    std::pair<Parser::Value, Parser::Value> Parser::Analyser::Cursor::peek_value_bundle(std::string_view opt) noexcept {
        auto backup_cursor = this->cursor;
        auto backup_processed = this->processed;
        auto result = next_value_bundle(opt);
        this->cursor = backup_cursor;
        this->processed = backup_processed;
        return result;
    }

    std::pair<Parser::Value, Parser::Value> Parser::Analyser::Cursor::next_value_bundle(std::string_view opt) noexcept {
        if (!matched) return {Value(false), Value(false)};
        if (!prefix.empty() && !processed) {
            return {Value(prefix), next()};
        }

        if (cursor == 0 || cursor - 1 >= data.size()) return {Value(false), Value(false)};
        auto s = data[cursor - 1];
        auto pos = s.find(opt);
        Value k(false);
        if (pos != std::string_view::npos) {
            k = Value(s.substr(0, pos));
        } else {
            k = Value(s);
        }

        auto px = this->prefix;
        auto py = this->opt_str;
        auto pp = this->processed;

        processed = false;
        prefix = k.data;
        opt_str = opt;
        Value v = next();

        prefix = px;
        opt_str = py;
        processed = pp;

        return {k, v};
    }

    void Parser::Analyser::Cursor::skip(usize n) noexcept {
        for (usize i = 0; i < n; ++i) {
            (void)next();
        }
    }

    Parser::Analyser Parser::Analyser::Cursor::commit() noexcept {
        if (!matched) return invalid();
        if (applied) {
            return Analyser(m_analyser.parser, *cached_data);
        }
        applied = true;
        cached_data.emplace(m_analyser.parser.resource);
        usize take_count = std::min(cursor, data.size());
        cached_data->insert(cached_data->end(), data.begin(), data.begin() + take_count);

        if (parent_index < m_analyser.inputs.size()) {
            usize erase_end = std::min(parent_index + take_count, m_analyser.inputs.size());
            m_analyser.inputs.erase(
                m_analyser.inputs.begin() + parent_index,
                m_analyser.inputs.begin() + erase_end
            );
        }
        return Analyser(m_analyser.parser, *cached_data);
    }

    Parser::Analyser Parser::Analyser::Cursor::sub() noexcept {
        if (!matched) return invalid();
        if (applied) {
            return Analyser(m_analyser.parser, *cached_data);
        }
        usize take_count = std::min(cursor, data.size());
        return Analyser(m_analyser.parser, data.subspan(0, take_count));
    }

    Parser::Analyser::Cursor Parser::Analyser::as_cursor(usize beg) noexcept {
        if (inputs.size() <= beg + 1) {
            return Cursor{*this, inputs, 0, false};
        }
        return Cursor{
            *this,
            std::span<std::string_view>(inputs).subspan(beg),
            beg,
            true
        };
    }

    std::span<std::string_view> Parser::Analyser::peek_remains() const noexcept {
        if (inputs.size() <= 1) return {};
        return std::span<std::string_view>(const_cast<std::string_view*>(inputs.data()) + 1, inputs.size() - 1);
    }

    pmr::vector<std::string_view> Parser::Analyser::remains() {
        if (inputs.size() <= 1) return pmr::vector<std::string_view>(parser.resource);
        pmr::vector<std::string_view> v(inputs.begin() + 1, inputs.end(), parser.resource);
        inputs.erase(inputs.begin() + 1, inputs.end());
        return v;
    }

    Parser::Analyser::Cursor Parser::Analyser::with_prefix(std::string_view data, std::string_view opt, usize beg) noexcept {
        for (usize i = beg; i < inputs.size(); ++i) {
            if (inputs[i].starts_with(data)) {
                auto c = Cursor{
                    *this,
                    std::span<std::string_view>(inputs).subspan(i),
                    i,
                    true
                };
                c.prefix = data;
                c.opt_str = opt;
                return c;
            }
        }
        return Cursor{*this, inputs, 0, false};
    }

    Parser::Analyser::Cursor Parser::Analyser::with_opt(std::string_view opt, usize beg) noexcept {
        for (usize i = beg; i < inputs.size(); ++i) {
            if (inputs[i] == opt) {
                return Cursor{
                    *this,
                    std::span<std::string_view>(inputs).subspan(i),
                    i,
                    true
                };
            }
        }
        return Cursor{*this, inputs, 0, false};
    }

    Parser::Analyser::Cursor Parser::Analyser::with_opts(std::span<const std::string_view> opts, usize beg) noexcept {
        for (auto v : opts) {
            if (auto c = with_opt(v, beg)) return c;
        }
        return Cursor{*this, inputs, 0, false};
    }

    Parser::Analyser::Cursor Parser::Analyser::with_opts(std::initializer_list<std::string_view> opts, usize beg) noexcept {
        return with_opts(std::span<const std::string_view>(opts.begin(), opts.end()), beg);
    }

    Parser::Analyser::Cursor Parser::Analyser::with_prefixes(std::span<const std::string_view> prefixes, std::string_view opt_str, usize beg) noexcept {
        for (auto v : prefixes) {
            if (auto c = with_prefix(v, opt_str, beg)) return c;
        }
        return Cursor{*this, inputs, 0, false};
    }

    Parser::Analyser::Cursor Parser::Analyser::with_prefixes(std::initializer_list<std::string_view> prefixes, std::string_view opt_str, usize beg) noexcept {
        return with_prefixes(std::span<const std::string_view>(prefixes.begin(), prefixes.end()), opt_str, beg);
    }

    bool Parser::Analyser::extract_first_flag(std::string_view flag, usize beg) {
        if (auto c = with_opt(flag, beg)) {
            c.commit();
            return true;
        }
        return false;
    }

    bool Parser::Analyser::extract_flag(std::string_view flag, usize beg) {
        bool ret = false;
        while (extract_first_flag(flag, beg)) {
            ret = true;
        }
        return ret;
    }

    bool Parser::Analyser::extract_first_any_flag(std::span<const std::string_view> flags, usize beg) {
        for (auto c : flags) {
            if (extract_first_flag(c, beg)) return true;
        }
        return false;
    }

    bool Parser::Analyser::extract_any_flag(std::span<const std::string_view> flags, usize beg) {
        bool val = false;
        for (auto c : flags) {
            val |= extract_flag(c, beg);
        }
        return val;
    }

    bool Parser::Analyser::extract_any_flag(std::initializer_list<const std::string_view> flags, usize beg) {
        return extract_any_flag(std::span<const std::string_view>(flags.begin(), flags.end()), beg);
    }

    Parser::Value Parser::Analyser::extract_an_option(std::string_view key, std::string_view opt, usize beg) {
        if (auto c = with_prefix(key, opt, beg)) {
            Value v = c.next();
            c.commit();
            return v;
        }
        return Value(false);
    }

    pmr::vector<Parser::Value> Parser::Analyser::extract_options(std::string_view key, std::string_view opt, usize beg) {
        pmr::vector<Value> ret(parser.resource);
        while (true) {
            if (auto c = with_prefix(key, opt, beg)) {
                ret.emplace_back(c.next());
                c.commit();
                continue;
            }
            break;
        }
        return ret;
    }

    Parser::Value Parser::Analyser::extract_any_option(std::initializer_list<std::string_view> keys, std::string_view opt, usize beg) {
        for (auto key : keys) {
            if (auto c = with_prefix(key, opt, beg)) {
                Value v = c.next();
                c.commit();
                return v;
            }
        }
        return Value(false);
    }

    pmr::vector<Parser::Analyser> Parser::analyse_pipe() {
        pmr::vector<Analyser> results(resource);
        auto start_it = args.begin();

        for (auto it = args.begin(); it != args.end(); ++it) {
            if (*it == "|") {
                pmr::vector<std::string_view> vs(resource);
                for (auto i = start_it; i < it; ++i) vs.push_back(*i);
                results.emplace_back(Analyser(*this, vs));
                start_it = it + 1;
            }
        }
        pmr::vector<std::string_view> vs(resource);
        for (auto i = start_it; i < args.end(); ++i) vs.push_back(*i);
        results.emplace_back(Analyser(*this, vs));
        return results;
    }

    void Parser::from_args(int argc, const char* argv[]) noexcept {
        head.clear();
        arg_full.clear();
        args.clear();

        for (int i = 0; i < argc; ++i) {
            pmr::string d(argv[i], resource);
            if (i != 0) {
                arg_full.push_back('\"');
                arg_full += str::escape(d, false, resource);
                arg_full.append("\" ");
            }
            args.emplace_back(std::move(d));
        }

        if (!args.empty()) {
            head = args[0];
        }
    }

    void Parser::parse(std::string_view data, bool slice_args) {
        head.clear();
        arg_full.clear();
        args.clear();

        std::string_view process = data;

        auto is_meta = [](char ch) noexcept {
            switch (ch) {
                case '|': case '&': case ';':
                case '(': case ')': case '<': case '>':
                    return true;
                default:
                    return false;
            }
        };

        auto extract_sector = [&]() -> std::string_view {
            usize pos = 0;
            bool string_mode = false;
            bool escape_mode = false;

            if (!process.empty() && process[0] == '"') {
                string_mode = true;
                process = process.substr(1);
            }

            for (; pos < process.size(); ++pos) {
                char ch = process[pos];
                if (!escape_mode && !string_mode) {
                    if (std::isspace(static_cast<unsigned char>(ch)) || ch == '"') break;
                    if (is_meta(ch)) {
                        if (pos == 0) ++pos;
                        break;
                    }
                }
                if (ch == '"' && string_mode && !escape_mode) break;

                if (ch == '\\') {
                    escape_mode = !escape_mode;
                } else if (escape_mode) {
                    escape_mode = false;
                }
            }

            std::string_view sector = process.substr(0, pos);
            if (pos < process.size()) {
                process = process.substr(pos + (string_mode ? 1 : 0));
            } else {
                process = "";
            }

            if (args.empty()) {
                arg_full = str::trim(process);
            }
            return sector;
        };

        while (true) {
            process = str::trim(process);
            if (process.empty()) break;
            auto sec = extract_sector();
            pmr::string d(str::unescape(sec, resource), resource);
            args.emplace_back(std::move(d));
            if (!slice_args) break;
        }

        if (!args.empty()) {
            head = args[0];
        }
    }

} // namespace alib6
