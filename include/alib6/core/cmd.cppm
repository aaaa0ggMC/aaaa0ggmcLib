/**
 * @file cmd.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 现代化命令行 CLI 交互解释器 (支持别名绑定、类型安全选项、层级路由)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <functional>

export module alib6.core:cmd;
import std;
import :types;
import :memory;
import :concepts;
import :error;
import :debug;
import :bstd;
import :str;
import :parser;
import :router;
import :storage;

namespace pmr = std::pmr;

export namespace alib6 {

    struct Command;

    /**
     * @brief 选项参数定义 (支持别名绑定与默认值)
     */
    struct Option {
        std::string_view name;            ///< 规范选项名称 (如 "port")
        std::string_view short_name{""};  ///< 短选项 (如 "-p")
        std::string_view long_name{""};   ///< 长选项 (如 "--port")
        std::string_view description{""}; ///< 选项描述
        std::string_view default_val{""}; ///< 默认值

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "Option(name=\"{}\", short=\"{}\", long=\"{}\")", name, short_name, long_name);
        }
    };

    /**
     * @brief 布尔开关 Flag 定义 (支持别名绑定)
     */
    struct Toggle {
        std::string_view name;            ///< 规范名称 (如 "verbose")
        std::string_view short_name{""};  ///< 短选项 (如 "-v")
        std::string_view long_name{""};   ///< 长选项 (如 "--verbose")
        std::string_view description{""}; ///< 开关描述

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "Toggle(name=\"{}\", short=\"{}\", long=\"{}\")", name, short_name, long_name);
        }
    };

    namespace detail {
        template<class T, class Cout, class Cin>
        concept IsRouteHandler = requires(T& t, const Cin& in) {
            { t(in) } -> std::convertible_to<Cout>;
        };
    }

    /**
     * @brief CLI 命令行解释与分发引擎
     */
    struct Command {
        Parser parser;
        Router router;

        pmr::vector<Option> registered_options;
        pmr::vector<Toggle> registered_toggles;

        pmr::unordered_map<
            pmr::string,
            usize,
            TransparentStringHash,
            TransparentStringEqual
        > alias_to_opt_idx;

        pmr::unordered_map<
            pmr::string,
            usize,
            TransparentStringHash,
            TransparentStringEqual
        > alias_to_toggle_idx;

        struct CommandInput;

        struct CommandOutput {
            int code{0};
            bool valid{true};
            bool should_terminate{false};

            CommandOutput() = default;
            CommandOutput(bool v) : valid(v) {}
            CommandOutput(int v) : code(v), valid(true) {}

            [[nodiscard]] explicit operator bool() const noexcept { return valid; }

            static CommandOutput terminate(int code = 0) noexcept {
                CommandOutput co(code);
                co.should_terminate = true;
                return co;
            }

            static CommandOutput with_code(int code) noexcept {
                return CommandOutput(code);
            }

            static CommandOutput no_output() noexcept {
                return CommandOutput(false);
            }
        };

        struct CommandInput {
            bool is_main{false};
            usize depth{0};
            const pmr::vector<pmr::unordered_set<std::string_view>>& found_toggles;
            const pmr::vector<pmr::unordered_map<std::string_view, pvalue_t>>& found_options;
            const pmr::unordered_map<pmr::string, pmr::string, TransparentStringHash, TransparentStringEqual>& keys;
            std::span<std::string_view> remains;
            std::span<std::string_view> routes;
            Command& cmd;

            [[nodiscard]] bool has(std::string_view name) const noexcept;
            [[nodiscard]] pvalue_t get(std::string_view name) const noexcept;

            template<class T>
            [[nodiscard]] auto get(std::string_view name, T&& default_val) const {
                auto val = get(name);
                if (!val) return static_cast<std::decay_t<T>>(default_val);
                return val.value_or(std::forward<T>(default_val));
            }

            [[nodiscard]] pvalue_t key(std::string_view k) const noexcept {
                auto it = keys.find(k);
                if (it != keys.end()) {
                    return pvalue_t(it->second);
                }
                return pvalue_t(false);
            }

            [[nodiscard]] std::span<std::string_view> args() const noexcept {
                return remains;
            }

            [[nodiscard]] pvalue_t arg(usize idx) const noexcept {
                if (idx < remains.size()) {
                    return pvalue_t(remains[idx]);
                }
                return pvalue_t(false);
            }
        };

        pmr::unordered_map<i64, std::function<CommandOutput(const CommandInput&)>> dispatchers;
        std::function<CommandOutput(const CommandInput&)> default_dispatcher;

    private:
        usize dispatch_max_id{0};

        panalyser_t judge_fn(
            pcursor_t* cursor,
            pmr::unordered_set<std::string_view>& found_t,
            pmr::unordered_map<std::string_view, pvalue_t>& found_o
        );

        pmr::vector<CommandOutput> dispatch_pipeline(bool remove_head);

        rdispatcher_t register_handler_internal(std::function<CommandOutput(const CommandInput&)> fn) {
            i64 id = static_cast<i64>(++dispatch_max_id);
            dispatchers[id] = std::move(fn);
            return rdispatcher_t(id);
        }

    public:
        explicit Command(memory_resource* mem = get_default_resource());

        void register_option(Option opt);
        void register_toggle(Toggle tog);

        void register_options(std::initializer_list<Option> opts) {
            for (const auto& opt : opts) register_option(opt);
        }

        void register_toggles(std::initializer_list<Toggle> togs) {
            for (const auto& tog : togs) register_toggle(tog);
        }

        template<detail::IsRouteHandler<CommandOutput, CommandInput> Fn>
        rdispatcher_t register_handler(Fn&& f) {
            return register_handler_internal([func = std::forward<Fn>(f)](const CommandInput& in) mutable -> CommandOutput {
                return func(in);
            });
        }

        template<detail::IsRouteHandler<CommandOutput, CommandInput> Fn>
        void register_default_handler(Fn&& f) {
            default_dispatcher = [func = std::forward<Fn>(f)](const CommandInput& in) mutable -> CommandOutput {
                return func(in);
            };
        }

        rgroup_t Group(std::string_view name, std::optional<rdispatcher_t> disp = std::nullopt) {
            return router.Group(name, disp);
        }

        bool add_route(std::string_view path, rdispatcher_t disp) {
            return router.add_route(path, disp);
        }

        template<detail::IsRouteHandler<CommandOutput, CommandInput> Fn>
        bool add_route(std::string_view path, Fn&& fn) {
            return router.add_route(path, register_handler(std::forward<Fn>(fn)));
        }

        pmr::vector<CommandOutput> from_str(std::string_view commands) {
            parser.parse(commands);
            return dispatch_pipeline(false);
        }

        pmr::vector<CommandOutput> from_args(int argc, const char** argv) {
            parser.from_args(argc, argv);
            return dispatch_pipeline(true);
        }

        template<bool has_color = false>
        struct HelpMessage {
            const Command& cmd;

            [[nodiscard]] HelpMessage<true> color() const noexcept {
                return HelpMessage<true>{cmd};
            }

            [[nodiscard]] pmr::string str(memory_resource* mem = get_default_resource()) const {
                return cmd.generate_help_string(mem);
            }

            template<class Target>
            void write_to_log(Target& target) const {
                target.append(str());
            }

            template<class CTX>
            CTX&& self_forward(CTX&& ctx) const {
                std::move(ctx) << str();
                return std::move(ctx);
            }
        };

        [[nodiscard]] HelpMessage<false> help() const noexcept {
            return HelpMessage<false>{*this};
        }

        [[nodiscard]] pmr::string generate_help_string(memory_resource* mem = get_default_resource()) const;
    };

    using command_in_t = const Command::CommandInput&;
    using command_out_t = Command::CommandOutput;

    template<bool has_color>
    inline std::ostream& operator<<(std::ostream& os, const Command::HelpMessage<has_color>& msg) {
        os << msg.str();
        return os;
    }

} // namespace alib6

export namespace std {
    template<bool has_color>
    struct formatter<alib6::Command::HelpMessage<has_color>> : formatter<string_view> {
        auto format(const alib6::Command::HelpMessage<has_color>& msg, format_context& ctx) const {
            auto s = msg.str();
            return formatter<string_view>::format(s, ctx);
        }
    };
} // namespace std
