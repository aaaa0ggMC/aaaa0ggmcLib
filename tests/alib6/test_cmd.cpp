/**
 * @file test_cmd.cpp
 * @brief alib6.core:cmd 现代化 CLI 解释器单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(CmdTest, OptionAliasAndTypeSafety) {
    alib6::Command cmd;

    cmd.register_option({
        .name = "port",
        .short_name = "-p",
        .long_name = "--port",
        .description = "Port to listen on",
        .default_val = "8080"
    });

    cmd.register_toggle({
        .name = "verbose",
        .short_name = "-v",
        .long_name = "--verbose",
        .description = "Verbose mode"
    });

    int captured_port = 0;
    bool captured_verbose = false;
    std::string captured_name;

    cmd.add_route("server/start", [&](const alib6::Command::CommandInput& in) {
        captured_port = in.get<int>("port", 0);
        captured_verbose = in.has("verbose");
        captured_name = in.arg(0).value_or("default_name");
        return alib6::Command::CommandOutput::with_code(0);
    });

    // 1. 测试短选项与位置参数
    auto out1 = cmd.from_str("server start -p 9000 -v my_app_instance");
    EXPECT_EQ(captured_port, 9000);
    EXPECT_TRUE(captured_verbose);
    EXPECT_EQ(captured_name, "my_app_instance");

    // 2. 测试长选项与默认值
    captured_verbose = false;
    auto out2 = cmd.from_str("server start --port=7070");
    EXPECT_EQ(captured_port, 7070);
    EXPECT_FALSE(captured_verbose);
    EXPECT_EQ(captured_name, "default_name");

    // 3. 测试 from_args (自动去除 argv[0] 应用程序名)
    const char* argv[] = {"my_app_bin", "server", "start", "-p", "8888", "--verbose", "cli_worker"};
    auto out3 = cmd.from_args(7, argv);
    EXPECT_EQ(captured_port, 8888);
    EXPECT_TRUE(captured_verbose);
    EXPECT_EQ(captured_name, "cli_worker");
}

TEST(CmdTest, DynamicRouteKeys) {
    alib6::Command cmd;

    std::string captured_user_id;
    std::string captured_action;

    cmd.add_route("user/{id}/{action}", [&](const alib6::Command::CommandInput& in) {
        captured_user_id = in.key("id").view();
        captured_action = in.key("action").view();
        return alib6::Command::CommandOutput::with_code(0);
    });

    auto out = cmd.from_str("user 12345 reset_password");
    EXPECT_EQ(captured_user_id, "12345");
    EXPECT_EQ(captured_action, "reset_password");
}

TEST(CmdTest, CommandHelpGeneration) {
    alib6::Command cmd;

    cmd.register_option({
        .name = "port",
        .short_name = "-p",
        .long_name = "--port",
        .description = "Service port number",
        .default_val = "8080"
    });

    cmd.register_toggle({
        .name = "verbose",
        .short_name = "-v",
        .long_name = "--verbose",
        .description = "Enable verbose logs"
    });

    cmd.add_route("service/start", [](const alib6::Command::CommandInput&) {
        return alib6::Command::CommandOutput::with_code(0);
    });
    cmd.add_route("service/stop", [](const alib6::Command::CommandInput&) {
        return alib6::Command::CommandOutput::with_code(0);
    });
    cmd.add_route("service/{id}/status", [](const alib6::Command::CommandInput&) {
        return alib6::Command::CommandOutput::with_code(0);
    });

    auto plain_help = cmd.help().str();
    EXPECT_FALSE(plain_help.empty());
    EXPECT_TRUE(plain_help.find("service") != std::string::npos);
    EXPECT_TRUE(plain_help.find("start") != std::string::npos);
    EXPECT_TRUE(plain_help.find("stop") != std::string::npos);
    EXPECT_TRUE(plain_help.find("{id}") != std::string::npos);
    EXPECT_TRUE(plain_help.find("Supported options:") != std::string::npos);
    EXPECT_TRUE(plain_help.find("Supported toggles:") != std::string::npos);
    EXPECT_TRUE(plain_help.find("8080") != std::string::npos);

    struct TestCmdTarget : public alib6::log::LogTarget {
        std::vector<std::string> records;
        void write(alib6::log::LogMsg& msg) override {
            records.emplace_back(msg.gen_composed());
        }
    };

    alib6::log::LoggerConfig cfg;
    cfg.consumer_count = 0;
    alib6::log::Logger logger(cfg);
    auto target = logger.append_mod<TestCmdTarget>("buf");
    alib6::log::LogFactory fac(logger);

    fac << cmd.help() << alib6::log::endlog;
    ASSERT_EQ(target->records.size(), 1);
    EXPECT_TRUE(target->records[0].find("service") != std::string::npos);
    EXPECT_TRUE(target->records[0].find("Supported options:") != std::string::npos);
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(CmdTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Command cmd(&tracker);

        cmd.register_option({
            .name = "host",
            .short_name = "-h",
            .long_name = "--host",
            .description = "Host address",
            .default_val = "localhost"
        });

        cmd.register_toggle({
            .name = "force",
            .short_name = "-f",
            .long_name = "--force"
        });

        bool executed = false;
        cmd.add_route("deploy/{env}", [&](const alib6::Command::CommandInput& in) {
            executed = true;
            EXPECT_EQ(in.key("env").view(), "production");
            EXPECT_EQ(in.get("host").view(), "10.0.0.1");
            EXPECT_TRUE(in.has("force"));
            return alib6::Command::CommandOutput::with_code(0);
        });

        auto out = cmd.from_str("deploy production --host=10.0.0.1 -f");
        EXPECT_TRUE(executed);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

// ---- 选项位置：前置选项、跨层扫描、"--" 终止符 ----------------------------------------

namespace {
struct Seen {
    bool called{false};
    bool json{false};
    bool quiet{false};
    std::string instance;
    std::string to;
    std::vector<std::string> args;
    std::size_t depth{0};
};

// 注册 `mods/list`、`mods/enable`、`plan` 三条路由，handler 把所见写进 Seen。
void setup_position_cmd(alib6::Command& cmd, Seen& seen) {
    cmd.register_toggles({{.name = "json", .short_name = "-j", .long_name = "--json"},
                          {.name = "quiet", .short_name = "-q", .long_name = "--quiet"}});
    cmd.register_options({{.name = "instance", .short_name = "-i", .long_name = "--instance"},
                          {.name = "to", .long_name = "--to"}});
    auto h = [&seen](const alib6::Command::CommandInput& in) {
        seen = Seen{};
        seen.called = true;
        seen.json = in.has("json");
        seen.quiet = in.has("quiet");
        seen.instance = std::string(in.get("instance").view());
        seen.to = std::string(in.get("to").view());
        seen.depth = in.depth;
        for (auto a : in.args()) seen.args.emplace_back(a);
        return alib6::Command::CommandOutput::with_code(0);
    };
    cmd.add_route("mods/list", h);
    cmd.add_route("mods/enable", h);
    cmd.add_route("plan", h);
}
}  // namespace

TEST(CmdTest, LeadingOptionsBeforeFirstRoute) {
    alib6::Command cmd;
    Seen seen;
    setup_position_cmd(cmd, seen);

    cmd.from_str("-j mods list");
    EXPECT_TRUE(seen.called);
    EXPECT_TRUE(seen.json);
    EXPECT_EQ(seen.depth, 1u);  // 仍然下降到 mods/list
    EXPECT_TRUE(seen.args.empty());

    cmd.from_str("-i /x mods list");
    EXPECT_TRUE(seen.called);
    EXPECT_EQ(seen.instance, "/x");

    cmd.from_str("--instance=/y mods list");
    EXPECT_EQ(seen.instance, "/y");

    cmd.from_str("-j -i /x -q mods enable ModA");
    EXPECT_TRUE(seen.json);
    EXPECT_TRUE(seen.quiet);
    EXPECT_EQ(seen.instance, "/x");
    EXPECT_EQ(seen.args, (std::vector<std::string>{"ModA"}));

    cmd.from_str("-q plan");
    EXPECT_TRUE(seen.called);
    EXPECT_TRUE(seen.quiet);
}

TEST(CmdTest, LeadingOptionsViaFromArgsRemoveHead) {
    alib6::Command cmd;
    Seen seen;
    setup_position_cmd(cmd, seen);
    const char* argv[] = {"prog", "-j", "-i", "/x", "mods", "enable", "ModA", "--to", "3"};
    cmd.from_args(9, argv);
    EXPECT_TRUE(seen.called);
    EXPECT_TRUE(seen.json);
    EXPECT_EQ(seen.instance, "/x");
    EXPECT_EQ(seen.to, "3");
    EXPECT_EQ(seen.args, (std::vector<std::string>{"ModA"}));
}

TEST(CmdTest, OptionsAnywhereAfterFirstRoute) {
    alib6::Command cmd;
    Seen seen;
    setup_position_cmd(cmd, seen);

    cmd.from_str("mods -j list");           // 路由 token 之间
    EXPECT_TRUE(seen.json);
    cmd.from_str("mods -i /x list -j");     // 前后混合
    EXPECT_EQ(seen.instance, "/x");
    EXPECT_TRUE(seen.json);
    cmd.from_str("mods enable ModA -j --to 3");  // 与位置参数混排
    EXPECT_EQ(seen.to, "3");
    EXPECT_EQ(seen.args, (std::vector<std::string>{"ModA"}));
}

TEST(CmdTest, DoubleDashTerminatesOptionParsing) {
    alib6::Command cmd;
    Seen seen;
    setup_position_cmd(cmd, seen);

    cmd.from_str("mods list -- -j");
    EXPECT_FALSE(seen.json);  // "--" 之后的 -j 是位置参数
    EXPECT_EQ(seen.args, (std::vector<std::string>{"-j"}));  // "--" 本身不出现在 args

    cmd.from_str("mods enable ModA -- --to 3 x");
    EXPECT_EQ(seen.to, "");
    EXPECT_EQ(seen.args, (std::vector<std::string>{"ModA", "--to", "3", "x"}));

    cmd.from_str("-j -- mods list");  // 前置 "--"：之后不再解析选项，但路由仍然下降
    EXPECT_TRUE(seen.json);
    EXPECT_EQ(seen.depth, 1u);
}

TEST(CmdTest, DefaultHandlerStillSeesOnlyOptions) {
    alib6::Command cmd;
    bool called = false;
    bool json = false;
    cmd.register_toggle({.name = "json", .short_name = "-j", .long_name = "--json"});
    cmd.register_default_handler([&](const alib6::Command::CommandInput& in) {
        called = true;
        json = in.has("json");
        return alib6::Command::CommandOutput::with_code(0);
    });
    cmd.from_str("-j");
    EXPECT_TRUE(called);
    EXPECT_TRUE(json);
}

TEST(CmdTest, OptionNameDoesNotSwallowSameNamedRoute) {
    // 回归：.name="instance" 曾把路由 token "instance" 当成选项吞掉，使 `instance init` 失效。
    alib6::Command cmd;
    std::string got_route_args;
    std::string got_instance;
    cmd.register_option({.name = "instance", .short_name = "-i", .long_name = "--instance"});
    cmd.add_route("instance/init", [&](const alib6::Command::CommandInput& in) {
        got_instance = std::string(in.get("instance").view());
        for (auto a : in.args()) got_route_args += std::string(a) + ",";
        return alib6::Command::CommandOutput::with_code(0);
    });
    auto outs = cmd.from_str("instance init --instance=/x extra");
    EXPECT_TRUE(!outs.empty() && static_cast<bool>(outs.front()));
    EXPECT_EQ(got_instance, "/x");
    EXPECT_EQ(got_route_args, "extra,");

    // 前置选项 + 同名路由
    got_instance.clear();
    got_route_args.clear();
    cmd.from_str("-i /y instance init");
    EXPECT_EQ(got_instance, "/y");
}

TEST(CmdTest, NameOnlyOptionStillMatchesBareToken) {
    // 兼容：只声明了 name（没有 long/short）的选项，仍可用裸名匹配。
    alib6::Command cmd;
    std::string got;
    cmd.register_option({.name = "port"});
    cmd.add_route("run", [&](const alib6::Command::CommandInput& in) {
        got = std::string(in.get("port").view());
        return alib6::Command::CommandOutput::with_code(0);
    });
    cmd.from_str("run port=9000");
    EXPECT_EQ(got, "9000");
}
