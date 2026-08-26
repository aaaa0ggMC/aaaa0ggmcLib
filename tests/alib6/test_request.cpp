#include <gtest/gtest.h>
#include "pmr_tracker.h"
#include <string>
#include <vector>

import alib6;

namespace {

    struct MyContext {
        int sum{0};
        std::vector<std::string> logs;
    };

    struct AddReq {
        int val;
        void handle_request(MyContext& ctx) const {
            ctx.sum += val;
        }
    };

    struct LogReq {
        std::string msg;
        void handle_request(MyContext& ctx) const {
            ctx.logs.push_back(msg);
        }
    };

} // namespace

TEST(RequestTest, MultiTypeBatchStealingAndProcessing) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::RequestManager<MyContext, AddReq, LogReq> mgr(&tracker);

        mgr.push_request<AddReq>(10);
        mgr.push_request<AddReq>(20);
        mgr.push_request<LogReq>("Action 1");
        mgr.push_request<AddReq>(30);
        mgr.push_request<LogReq>("Action 2");

        EXPECT_TRUE(mgr.until());

        MyContext ctx;
        std::size_t processed = mgr.process_batch(ctx);
        EXPECT_EQ(processed, 5);
        EXPECT_EQ(ctx.sum, 60);
        EXPECT_EQ(ctx.logs.size(), 2);
        EXPECT_EQ(ctx.logs[0], "Action 1");
        EXPECT_EQ(ctx.logs[1], "Action 2");

        // 再次处理应返回 0
        EXPECT_EQ(mgr.process_batch(ctx), 0);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
