#include <gtest/gtest.h>
#include <generator>
#include <vector>
#include <chrono>
#include <thread>

import alib6;

namespace {

    std::generator<int> count_up(int max) {
        for (int i = 1; i <= max; ++i) {
            co_yield i;
        }
    }

} // namespace

TEST(CoTest, BasicTaskAndGeneratorIteration) {
    auto t = count_up(3);
    alib6::co::Task task(std::move(t));

    EXPECT_TRUE(task.should_next());
    task.next();
    EXPECT_EQ(*task.current.value(), 1);

    EXPECT_TRUE(task.should_next());
    task.next();
    EXPECT_EQ(*task.current.value(), 2);

    EXPECT_TRUE(task.should_next());
    task.next();
    EXPECT_EQ(*task.current.value(), 3);

    task.next();
    EXPECT_FALSE(task.should_next());
}

TEST(CoTest, SignalAndWaitGroup) {
    // 1. Signal
    alib6::co::Signal sig;
    EXPECT_FALSE(sig.ready());
    EXPECT_FALSE(sig.until());

    std::jthread th1([&sig] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sig.fire();
    });

    auto gen = alib6::nap0();
    alib6::co::Task task(std::move(gen));
    alib6::co::wait_until(task, sig);
    EXPECT_TRUE(sig.ready());

    // 2. WaitGroup
    alib6::co::WaitGroup wg;
    {
        auto g1 = wg.make_guard();
        auto g2 = wg.make_guard();
        EXPECT_FALSE(wg.ready());
    }
    EXPECT_TRUE(wg.ready());
}

TEST(CoTest, RaceAndAny) {
    alib6::co::Signal sig1;
    alib6::co::Signal sig2;

    std::jthread th([&sig2] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sig2.fire();
    });

    auto gen = alib6::nap0();
    alib6::co::Task task(std::move(gen));
    std::size_t winner = alib6::co::any(task, sig1, sig2);
    EXPECT_EQ(winner, 1);
    EXPECT_TRUE(sig2.ready());
}

TEST(CoTest, CombineTasks) {
    std::vector<int> out1;
    std::vector<int> out2;

    auto gen1 = [&out1]() -> std::generator<int> {
        out1.push_back(10);
        co_yield 0;
        out1.push_back(20);
        co_yield 0;
    };

    auto gen2 = [&out2]() -> std::generator<int> {
        out2.push_back(100);
        co_yield 0;
        out2.push_back(200);
        co_yield 0;
    };

    auto combined = alib6::co::combine_tasks(gen1(), gen2());
    combined.next();
    EXPECT_EQ(out1.size(), 1);
    EXPECT_EQ(out2.size(), 1);

    combined.next();
    EXPECT_EQ(out1.size(), 2);
    EXPECT_EQ(out2.size(), 2);
}
