/**
 * @file parser.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 命令行词法分词器与渐进式事务游标分析器 (接口定义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <charconv>

export module alib6.core:parser;
import std;
import :types;
import :memory;
import :concepts;
import :error;
import :debug;
import :str;

namespace pmr = std::pmr;

export namespace alib6 {

    /**
     * @brief 命令行分词与渐进式词法分析器
     */
    struct Parser {
        memory_resource* resource{get_default_resource()};
        pmr::string head;                             ///< 命令头
        pmr::string arg_full;                         ///< 去除命令头后的完整参数串
        pmr::vector<pmr::string> args;                ///< 结构化分词列表

        explicit Parser(memory_resource* mem = get_default_resource());

        /**
         * @brief 解析完整命令行字符串
         * @param cmd 命令行字符串
         * @param slice_args 是否自动切分后续参数
         */
        void parse(std::string_view cmd, bool slice_args = true);

        /**
         * @brief 从标准 argc/argv 构建解析结果
         */
        void from_args(int argc, const char* argv[]) noexcept;

        [[nodiscard]] bool match(std::string_view chead) const noexcept {
            return chead == head;
        }

        [[nodiscard]] bool match(std::span<const std::string_view> cheads) const noexcept {
            for (auto h : cheads) {
                if (match(h)) return true;
            }
            return false;
        }

        [[nodiscard]] bool match(std::initializer_list<std::string_view> cheads) const noexcept {
            return match(std::span<const std::string_view>(cheads.begin(), cheads.end()));
        }

        struct Analyser;

        /**
         * @brief 提取出的轻量值包装器
         */
        struct Value {
            bool valid{false};
            std::string_view data;

            Value() = default;
            Value(bool v) : valid(v), data("") {}

            template<Cast<std::string_view> T>
            Value(T&& s) : valid(true), data(std::forward<T>(s)) {}

            [[nodiscard]] explicit operator bool() const noexcept { return valid; }
            [[nodiscard]] std::string_view view() const noexcept { return data; }

            template<class T>
            [[nodiscard]] auto as(ErrorWrapper err = {}) const {
                if (!valid) {
                    err.report("Parser::Value is invalid!");
                    return T{};
                }
                return ext::to_T<T>(data, nullptr, err);
            }

            template<class T>
            [[nodiscard]] auto value_or(T&& default_val) const {
                using CleanT = std::decay_t<T>;
                if constexpr (std::is_same_v<CleanT, const char*> || std::is_same_v<CleanT, char*>) {
                    if (!valid) return std::string_view(default_val);
                    return data;
                } else if constexpr (std::is_same_v<CleanT, std::string_view>) {
                    if (!valid) return std::string_view(default_val);
                    return data;
                } else if constexpr (std::is_same_v<CleanT, std::string> || std::is_same_v<CleanT, pmr::string>) {
                    if (!valid) return CleanT(default_val);
                    return CleanT(data);
                } else if constexpr (std::is_arithmetic_v<CleanT>) {
                    if (!valid) return static_cast<CleanT>(default_val);
                    std::from_chars_result r;
                    auto v = ext::to_T<CleanT>(data, &r);
                    if (r.ec != std::errc() || r.ptr != data.data() + data.size()) {
                        return static_cast<CleanT>(default_val);
                    }
                    return v;
                } else {
                    if (!valid) return static_cast<CleanT>(default_val);
                    return ext::to_T<CleanT>(data);
                }
            }

            template<class T>
            [[nodiscard]] std::optional<T> expect() const {
                if (!valid) return std::nullopt;
                if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
                    std::from_chars_result r;
                    auto v = ext::to_T<std::decay_t<T>>(data, &r);
                    if (r.ec != std::errc() || r.ptr != data.data() + data.size()) {
                        return std::nullopt;
                    }
                    return v;
                }
                return { as<T>() };
            }

            template<class Target>
            void write_to_log(Target& target) const {
                if (valid) {
                    target.append(data);
                } else {
                    target.append("<invalid>");
                }
            }
        };

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "Parser(head=\"{}\", args_count={})", head, args.size());
        }

        /**
         * @brief 渐进式事务分析器
         */
        struct Analyser {
            Parser& parser;
            pmr::vector<std::string_view> inputs;

            friend struct Parser;

            explicit Analyser(Parser& p);
            Analyser(Parser& p, std::span<std::string_view> slice);

            /**
             * @brief 事务游标 (支持窥视、提取、提交与回滚)
             */
            struct Cursor {
                Analyser& m_analyser;
                std::span<std::string_view> data;
                usize parent_index{0};
                usize cursor{1};

                std::string_view prefix{""};
                std::string_view opt_str{""};

                bool matched{false};
                bool processed{false};
                bool applied{false};

                std::optional<pmr::vector<std::string_view>> cached_data{std::nullopt};

                Cursor(Analyser& ana, std::span<std::string_view> d, usize pi, bool mt, usize beg = 1);

                [[nodiscard]] explicit operator bool() const noexcept {
                    return matched && cursor <= data.size();
                }

                Cursor operator||(const Cursor& c) const noexcept;

                [[nodiscard]] Value head() const noexcept {
                    return data.empty() ? Value(false) : Value(data[0]);
                }

                void abort() noexcept { matched = false; }

                [[nodiscard]] bool match(std::string_view str) noexcept;
                [[nodiscard]] bool match(std::span<const std::string_view> strs) noexcept;

                [[nodiscard]] bool reached_end() const noexcept {
                    return cursor >= data.size();
                }

                [[nodiscard]] Analyser invalid() noexcept;
                Value next() noexcept;
                [[nodiscard]] Value peek() const noexcept;

                std::pair<Value, Value> next_value_bundle(std::string_view opt) noexcept;
                [[nodiscard]] std::pair<Value, Value> peek_value_bundle(std::string_view opt) noexcept;

                void skip(usize n = 1) noexcept;
                Analyser commit() noexcept;
                Analyser sub() noexcept;
            };

            [[nodiscard]] Cursor as_cursor(usize beg = 0) noexcept;
            [[nodiscard]] std::span<std::string_view> peek_remains() const noexcept;
            [[nodiscard]] pmr::vector<std::string_view> remains();

            [[nodiscard]] explicit operator bool() const noexcept {
                return inputs.size() > 1;
            }

            Cursor with_opt(std::string_view opt, usize beg = 1) noexcept;
            Cursor with_prefix(std::string_view data, std::string_view opt = "", usize beg = 1) noexcept;

            Cursor with_opts(std::span<const std::string_view> opts, usize beg = 1) noexcept;
            Cursor with_opts(std::initializer_list<std::string_view> opts, usize beg = 1) noexcept;

            Cursor with_prefixes(std::span<const std::string_view> prefixes, std::string_view opt_str = "", usize beg = 1) noexcept;
            Cursor with_prefixes(std::initializer_list<std::string_view> prefixes, std::string_view opt_str = "", usize beg = 1) noexcept;

            bool extract_first_flag(std::string_view flag, usize beg = 1);
            bool extract_flag(std::string_view flag, usize beg = 1);
            bool extract_first_any_flag(std::span<const std::string_view> flags, usize beg = 1);
            bool extract_any_flag(std::span<const std::string_view> flags, usize beg = 1);
            bool extract_any_flag(std::initializer_list<const std::string_view> flags, usize beg = 1);

            Value extract_an_option(std::string_view key, std::string_view opt = "=", usize beg = 1);
            pmr::vector<Value> extract_options(std::string_view key, std::string_view opt = "=", usize beg = 1);
            Value extract_any_option(std::initializer_list<std::string_view> keys, std::string_view opt = "=", usize beg = 1);
        };

        [[nodiscard]] Analyser analyse() {
            return Analyser(*this);
        }

        [[nodiscard]] pmr::vector<Analyser> analyse_pipe();

        [[nodiscard]] Analyser operator()() {
            return analyse();
        }
    };

    using pvalue_t = Parser::Value;
    using panalyser_t = Parser::Analyser;
    using pcursor_t = Parser::Analyser::Cursor;

} // namespace alib6
