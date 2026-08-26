/**
 * @file test_storage.cpp
 * @brief alib6.core:storage 线性自由链表槽位复用存储与位图单测 (含 PMR 内存池隔离与泄漏检测)
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

using namespace alib6::storage;

TEST(StorageTest, MonoBitSetOperations) {
    MonoBitSet bs;
    EXPECT_TRUE(bs.none());
    EXPECT_EQ(bs.count_1(), 0);

    bs.set(5);
    bs.set(63);
    bs.set(64); // 跨 64 位 word
    bs.set(130);

    EXPECT_FALSE(bs.none());
    EXPECT_TRUE(bs.get(5));
    EXPECT_TRUE(bs.get(63));
    EXPECT_TRUE(bs.get(64));
    EXPECT_TRUE(bs.get(130));
    EXPECT_FALSE(bs.get(6));
    EXPECT_FALSE(bs.get(100));

    EXPECT_EQ(bs.count_1(), 4);

    // 测试 find_next_1
    EXPECT_EQ(bs.find_next_1(0), 5);
    EXPECT_EQ(bs.find_next_1(6), 63);
    EXPECT_EQ(bs.find_next_1(64), 64);
    EXPECT_EQ(bs.find_next_1(65), 130);
    EXPECT_EQ(bs.find_next_1(131), std::nullopt);

    // 测试 for_each_1
    std::vector<size_t> ones;
    bs.for_each_1([&](size_t pos) {
        ones.push_back(pos);
    });
    ASSERT_EQ(ones.size(), 4);
    EXPECT_EQ(ones[0], 5);
    EXPECT_EQ(ones[1], 63);
    EXPECT_EQ(ones[2], 64);
    EXPECT_EQ(ones[3], 130);

    bs.reset(63);
    EXPECT_FALSE(bs.get(63));
    EXPECT_EQ(bs.count_1(), 3);
}

struct TestCleanupItem {
    int id{0};
    static inline int cleanup_count{0};

    void d_cleanup() {
        ++cleanup_count;
    }
};

struct TestResetableItem {
    int id{0};
    std::string name;
    static inline int reset_count{0};

    TestResetableItem(int i, std::string n) : id(i), name(std::move(n)) {}

    void reset(int i, std::string n) {
        id = i;
        name = std::move(n);
        ++reset_count;
    }
};

TEST(StorageTest, FreelistSlotReuseAndReset) {
    TestResetableItem::reset_count = 0;
    FreelistLinearStorage<TestResetableItem> storage;

    bool is_new = false;
    size_t idx0 = 0;
    size_t idx1 = 0;
    size_t idx2 = 0;

    auto& item0 = storage.try_next_with_index(is_new, idx0, 100, "item0");
    EXPECT_TRUE(is_new);
    EXPECT_EQ(idx0, 0);
    EXPECT_EQ(item0.id, 100);

    auto& item1 = storage.try_next_with_index(is_new, idx1, 200, "item1");
    EXPECT_TRUE(is_new);
    EXPECT_EQ(idx1, 1);

    auto& item2 = storage.try_next_with_index(is_new, idx2, 300, "item2");
    EXPECT_TRUE(is_new);
    EXPECT_EQ(idx2, 2);

    EXPECT_EQ(storage.occupied_count(), 3);
    EXPECT_EQ(storage.free_count(), 0);

    // 释放中间的 idx1 (槽位 1)
    storage.remove(idx1);
    EXPECT_TRUE(storage.is_free(idx1));
    EXPECT_FALSE(storage.is_occupied(idx1));
    EXPECT_EQ(storage.occupied_count(), 2);
    EXPECT_EQ(storage.free_count(), 1);

    // 下一次 try_next 应当精准复用槽位 1 并调用 reset
    size_t reused_idx = 0;
    auto& reused_item = storage.try_next_with_index(is_new, reused_idx, 999, "reused_item");
    EXPECT_FALSE(is_new); // 来自空闲槽位复用
    EXPECT_EQ(reused_idx, 1);
    EXPECT_EQ(reused_item.id, 999);
    EXPECT_EQ(reused_item.name, "reused_item");
    EXPECT_EQ(TestResetableItem::reset_count, 1); // 成功调用 reset

    EXPECT_EQ(storage.occupied_count(), 3);
    EXPECT_EQ(storage.free_count(), 0);
}

TEST(StorageTest, ForEachIterationAndCleanupHook) {
    TestCleanupItem::cleanup_count = 0;
    {
        FreelistLinearStorage<TestCleanupItem> storage;
        storage.next(TestCleanupItem{.id = 1});
        storage.next(TestCleanupItem{.id = 2});
        storage.next(TestCleanupItem{.id = 3});

        storage.remove(1); // 移除索引 1
        EXPECT_EQ(TestCleanupItem::cleanup_count, 1);

        // 遍历测试：应该只遍历索引 0 和 2
        std::vector<int> visited_ids;
        std::vector<size_t> visited_indices;
        storage.for_each([&](size_t idx, TestCleanupItem& item) {
            visited_indices.push_back(idx);
            visited_ids.push_back(item.id);
        });

        ASSERT_EQ(visited_ids.size(), 2);
        EXPECT_EQ(visited_indices[0], 0);
        EXPECT_EQ(visited_ids[0], 1);
        EXPECT_EQ(visited_indices[1], 2);
        EXPECT_EQ(visited_ids[1], 3);

        storage.clear();
        // clear 时对剩下的 0 和 2 执行 cleanup
        EXPECT_EQ(TestCleanupItem::cleanup_count, 3);
    }
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(StorageTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        FreelistLinearStorage<std::string> storage(10, &tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);

        for (int i = 0; i < 50; ++i) {
            storage.next("Storage PMR element string with long payload...");
        }
        for (size_t i = 10; i < 40; ++i) {
            storage.remove(i);
        }
        for (int i = 0; i < 20; ++i) {
            bool is_new = false;
            storage.try_next(is_new, "Reused payload string");
        }
    }

    // 析构离开作用域后，内部所有 vector 与 bitset 必须 100% 归还内存
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
