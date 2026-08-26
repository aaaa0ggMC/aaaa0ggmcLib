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
