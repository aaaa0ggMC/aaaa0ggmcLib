/**
 * @file test_router.cpp
 * @brief alib6.core:router 树形路由与 Any 动态参数匹配单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

TEST(RouterTest, BasicRouteMatch) {
    alib6::Router router;

    router.add_route("user/profile", alib6::rdispatcher_t(101));
    router.add_route("user/settings", alib6::rdispatcher_t(102));
    router.add_route("system/reboot", alib6::rdispatcher_t(201));

    alib6::Parser parser;
    parser.parse("cli user profile");

    auto res = router.match(
        parser,
        [](alib6::pcursor_t* p) { return p->invalid(); },
        [](alib6::panalyser_t*) {}
    );

    ASSERT_EQ(res.dispatches.size(), 2);
    EXPECT_EQ(res.dispatches.back().id, 101);
}

TEST(RouterTest, DynamicAnyRouteParam) {
    alib6::Router router;
    router.add_route("user/{uid}/detail", alib6::rdispatcher_t(300));

    alib6::Parser parser;
    parser.parse("app user 98765 detail");

    auto res = router.match(
        parser,
        [](alib6::pcursor_t* p) { return p->invalid(); },
        [](alib6::panalyser_t*) {}
    );

    ASSERT_EQ(res.dispatches.size(), 3);
    EXPECT_EQ(res.dispatches.back().id, 300);

    auto it = res.keys.find("uid");
    ASSERT_NE(it, res.keys.end());
    EXPECT_EQ(it->second, "98765");
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(RouterTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Router router(&tracker);
        router.add_route("api/v1/{service}/start", alib6::rdispatcher_t(1));
        router.add_route("api/v1/{service}/stop", alib6::rdispatcher_t(2));
        router.add_route("api/v1/{service}/status", alib6::rdispatcher_t(3));
        EXPECT_GT(tracker.allocated_bytes(), 0);

        alib6::Parser parser(&tracker);
        parser.parse("cli api v1 auth_service status");

        auto res = router.match(
            parser,
            [](alib6::pcursor_t* p) { return p->invalid(); },
            [](alib6::panalyser_t*) {}
        );

        EXPECT_EQ(res.dispatches.back().id, 3);
        EXPECT_EQ(res.keys.find("service")->second, "auth_service");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
