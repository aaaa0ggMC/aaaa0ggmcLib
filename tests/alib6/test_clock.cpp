#include <gtest/gtest.h>
#include <chrono>
#include <thread>

import alib6;

TEST(ClockTest, ClockStartPauseResumeStop) {
    alib6::Clock clk(false);
    EXPECT_EQ(clk.status(), alib6::Clock::Status::Stopped);
    EXPECT_EQ(clk.get_all(), 0.0);

    clk.start();
    EXPECT_EQ(clk.status(), alib6::Clock::Status::Running);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_GT(clk.get_all(), 10.0);

    clk.pause();
    EXPECT_EQ(clk.status(), alib6::Clock::Status::Paused);
    double paused_time = clk.get_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_DOUBLE_EQ(clk.get_all(), paused_time);

    clk.resume();
    EXPECT_EQ(clk.status(), alib6::Clock::Status::Running);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_GT(clk.get_all(), paused_time + 10.0);

    clk.stop();
    EXPECT_EQ(clk.status(), alib6::Clock::Status::Stopped);
    EXPECT_EQ(clk.get_all(), 0.0);
}

TEST(ClockTest, TriggerAndTimer) {
    alib6::Clock clk(true);
    alib6::Trigger trig(clk, 20.0); // 20ms

    EXPECT_FALSE(trig.test(false));
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_TRUE(trig.test(true));

    // Timer
    alib6::Timer timer(15.0); // 15ms
    EXPECT_FALSE(timer.trigger(false));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_TRUE(timer.trigger(true));
}

TEST(ClockTest, RateLimiterFPS) {
    alib6::RateLimiter limiter(100.0); // 10ms per frame
    alib6::Clock clk(true);

    int count = 0;
    for (int i = 0; i < 3; ++i) {
        limiter.wait();
        ++count;
    }
    EXPECT_EQ(count, 3);
    EXPECT_GT(clk.get_all(), 15.0);
}
