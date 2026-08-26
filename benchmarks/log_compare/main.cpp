#include <iostream>
#include <vector>
#include <chrono>
#include <print>
#include <thread>
#include <memory>
#include <memory_resource>

#include <alib5/alogger.h>
import alib6;

struct Alib5NullTarget : public alib5::LogTarget {
    void write(alib5::LogMsg&) override {}
};

struct Alib6NullTarget : public alib6::log::LogTarget {
    void write(alib6::log::LogMsg&) override {}
};

int main() {
    std::println("==========================================================================================");
    std::println("                 alib5 vs alib6 日志系统细粒度对照基准测试 (-O3 Release)                  ");
    std::println("==========================================================================================\n");

    constexpr int warm_up = 10000;
    constexpr int runs = 250000;

    std::pmr::synchronized_pool_resource pool6;

    // ==========================================================================================
    // 场景 A: 纯 Raw PMR 消息入队 (push_message_pmr，不包含 operator<< 流式拼接与格式化)
    // ==========================================================================================
    {
        // alib5
        alib5::LoggerConfig cfg5;
        cfg5.consumer_count = 1;
        alib5::Logger logger5(cfg5);
        logger5.append_mod<Alib5NullTarget>("null");
        alib5::LogMsgConfig mcfg5;

        for (int i = 0; i < warm_up; ++i) {
            std::pmr::string s("Worker 100 status: true", logger5.msg_str_alloc);
            logger5.push_message_pmr(0, "A5", s, mcfg5);
        }
        logger5.flush();

        auto t0_5 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            std::pmr::string s("Worker 100 status: true", logger5.msg_str_alloc);
            logger5.push_message_pmr(0, "A5", s, mcfg5);
        }
        auto t1_5 = std::chrono::high_resolution_clock::now();
        logger5.flush();
        double ms5 = std::chrono::duration<double, std::milli>(t1_5 - t0_5).count();

        // alib6
        alib6::log::LoggerConfig cfg6{.consumer_count = 1};
        alib6::log::Logger logger6(cfg6, &pool6);
        logger6.append_mod<Alib6NullTarget>("null");
        alib6::log::LogMsgConfig mcfg6;

        for (int i = 0; i < warm_up; ++i) {
            std::pmr::string s("Worker 100 status: true", &pool6);
            logger6.push_message_pmr(0, "A6", std::move(s), mcfg6);
        }
        logger6.flush();

        auto t0_6 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            std::pmr::string s("Worker 100 status: true", &pool6);
            logger6.push_message_pmr(0, "A6", std::move(s), mcfg6);
        }
        auto t1_6 = std::chrono::high_resolution_clock::now();
        logger6.flush();
        double ms6 = std::chrono::duration<double, std::milli>(t1_6 - t0_6).count();

        std::println("[场景 A] 纯 Raw 消息入队 (push_message_pmr，不包含流式 << 格式化):");
        std::println("  • alib5: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms5 * 1e6) / runs, (runs / ms5) / 10.0);
        std::println("  • alib6: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms6 * 1e6) / runs, (runs / ms6) / 10.0);
        std::println("------------------------------------------------------------------------------------------\n");
    }

    // ==========================================================================================
    // 场景 B: 纯定长字符串流式推送 (lg << "Hello World" << endlog)
    // ==========================================================================================
    {
        alib5::LoggerConfig cfg5;
        cfg5.consumer_count = 1;
        alib5::Logger logger5(cfg5);
        logger5.append_mod<Alib5NullTarget>("null");
        alib5::LogFactory lg5(logger5, "A5");

        for (int i = 0; i < warm_up; ++i) {
            lg5 << "Hello World" << alib5::endlog;
        }
        logger5.flush();

        auto t0_5 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            lg5 << "Hello World" << alib5::endlog;
        }
        auto t1_5 = std::chrono::high_resolution_clock::now();
        logger5.flush();
        double ms5 = std::chrono::duration<double, std::milli>(t1_5 - t0_5).count();

        alib6::log::LoggerConfig cfg6{.consumer_count = 1};
        alib6::log::Logger logger6(cfg6, &pool6);
        logger6.append_mod<Alib6NullTarget>("null");
        alib6::log::LogFactory lg6(logger6, "A6");

        for (int i = 0; i < warm_up; ++i) {
            lg6 << "Hello World" << alib6::log::endlog;
        }
        logger6.flush();

        auto t0_6 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            lg6 << "Hello World" << alib6::log::endlog;
        }
        auto t1_6 = std::chrono::high_resolution_clock::now();
        logger6.flush();
        double ms6 = std::chrono::duration<double, std::milli>(t1_6 - t0_6).count();

        std::println("[场景 B] 纯定长字符串流式推送 (lg << \"Hello World\" << endlog):");
        std::println("  • alib5: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms5 * 1e6) / runs, (runs / ms5) / 10.0);
        std::println("  • alib6: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms6 * 1e6) / runs, (runs / ms6) / 10.0);
        std::println("------------------------------------------------------------------------------------------\n");
    }

    // ==========================================================================================
    // 场景 C: 多字段格式化流式推送 (lg << "Worker " << 100 << " status: " << true << endlog)
    // ==========================================================================================
    {
        alib5::LoggerConfig cfg5;
        cfg5.consumer_count = 1;
        alib5::Logger logger5(cfg5);
        logger5.append_mod<Alib5NullTarget>("null");
        alib5::LogFactory lg5(logger5, "A5");

        for (int i = 0; i < warm_up; ++i) {
            lg5 << "Worker " << 100 << " status: " << true << alib5::endlog;
        }
        logger5.flush();

        auto t0_5 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            lg5 << "Worker " << 100 << " status: " << true << alib5::endlog;
        }
        auto t1_5 = std::chrono::high_resolution_clock::now();
        logger5.flush();
        double ms5 = std::chrono::duration<double, std::milli>(t1_5 - t0_5).count();

        alib6::log::LoggerConfig cfg6{.consumer_count = 1};
        alib6::log::Logger logger6(cfg6, &pool6);
        logger6.append_mod<Alib6NullTarget>("null");
        alib6::log::LogFactory lg6(logger6, "A6");

        for (int i = 0; i < warm_up; ++i) {
            lg6 << "Worker " << 100 << " status: " << true << alib6::log::endlog;
        }
        logger6.flush();

        auto t0_6 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            lg6 << "Worker " << 100 << " status: " << true << alib6::log::endlog;
        }
        auto t1_6 = std::chrono::high_resolution_clock::now();
        logger6.flush();
        double ms6 = std::chrono::duration<double, std::milli>(t1_6 - t0_6).count();

        std::println("[场景 C] 包含 2 个变量格式化的流式推送 (lg << \"Worker \" << 100 << \" status: \" << true << endlog):");
        std::println("  • alib5: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms5 * 1e6) / runs, (runs / ms5) / 10.0);
        std::println("  • alib6: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms6 * 1e6) / runs, (runs / ms6) / 10.0);
        std::println("------------------------------------------------------------------------------------------\n");
    }

    // ==========================================================================================
    // 场景 D: 同步直写模式 (consumer_count = 0)
    // ==========================================================================================
    {
        alib5::LoggerConfig cfg5;
        cfg5.consumer_count = 0;
        alib5::Logger logger5(cfg5);
        logger5.append_mod<Alib5NullTarget>("null");
        alib5::LogFactory lg5(logger5, "A5Sync");

        auto t0_5 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            lg5 << "Worker " << 100 << " status: " << true << alib5::endlog;
        }
        auto t1_5 = std::chrono::high_resolution_clock::now();
        double ms5 = std::chrono::duration<double, std::milli>(t1_5 - t0_5).count();

        alib6::log::LoggerConfig cfg6{.consumer_count = 0};
        alib6::log::Logger logger6(cfg6, &pool6);
        logger6.append_mod<Alib6NullTarget>("null");
        alib6::log::LogFactory lg6(logger6, "A6Sync");

        auto t0_6 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < runs; ++i) {
            lg6 << "Worker " << 100 << " status: " << true << alib6::log::endlog;
        }
        auto t1_6 = std::chrono::high_resolution_clock::now();
        double ms6 = std::chrono::duration<double, std::milli>(t1_6 - t0_6).count();

        std::println("[场景 D] 同步直写模式 (consumer_count = 0):");
        std::println("  • alib5: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms5 * 1e6) / runs, (runs / ms5) / 10.0);
        std::println("  • alib6: {:.2f} ns/条 (吞吐: {:.2f} 万条/s)", (ms6 * 1e6) / runs, (runs / ms6) / 10.0);
        std::println("------------------------------------------------------------------------------------------\n");
    }

    return 0;
}
