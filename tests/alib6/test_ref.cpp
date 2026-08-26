/**
 * @file test_ref.cpp
 * @brief alib6.core:ref 容器安全引用与操作符穿透单测
 */
#include <gtest/gtest.h>
import std;
import alib6;

TEST(RefTest, SingleRefBasic) {
    std::vector<int> vec = {10, 20, 30};
    auto r = alib6::ref(vec, 1);
    
    EXPECT_TRUE(r.valid());
    EXPECT_EQ(*r, 20);
    EXPECT_EQ(r.get_index(), 1);

    *r = 200;
    EXPECT_EQ(vec[1], 200);

    // 隐式转换为引用
    int& raw_ref = r;
    raw_ref = 300;
    EXPECT_EQ(vec[1], 300);
}

TEST(RefTest, ReallocationSafety) {
    std::vector<int> vec = {1, 2, 3};
    auto r = alib6::ref(vec, 0);

    // 大幅扩容触发 vector 内部指针搬家
    vec.reserve(10000);
    for (int i = 4; i <= 500; ++i) {
        vec.push_back(i);
    }

    // 传统 C++ int& 会悬垂崩溃，但 RefWrapper 依然有效
    EXPECT_EQ(*r, 1);
    *r = 999;
    EXPECT_EQ(vec[0], 999);
}

TEST(RefTest, ExplicitNavigation) {
    std::vector<int> vec = {10, 20, 30, 40};
    auto r = alib6::ref(vec, 1);

    r.next_element();
    EXPECT_EQ(*r, 30);
    EXPECT_EQ(r.get_index(), 2);

    r.prev_element();
    EXPECT_EQ(*r, 20);

    r.advance(2);
    EXPECT_EQ(*r, 40);

    auto r_offset = r.offset_element(-3);
    EXPECT_EQ(*r_offset, 10);
    EXPECT_EQ(r.get_index(), 3); // 原对象不变
}

TEST(RefTest, OperatorPassThrough) {
    // 1. 下标 [] 穿透测试
    std::vector<std::string> str_vec = {"hello", "world"};
    auto str_ref = alib6::ref(str_vec, 0);
    EXPECT_EQ(str_ref[0], 'h');
    EXPECT_EQ(str_ref[4], 'o');
    str_ref[0] = 'H';
    EXPECT_EQ(str_vec[0], "Hello");

    // 2. 调用 () 穿透测试
    std::vector<std::function<int(int, int)>> fn_vec = {
        [](int a, int b) { return a + b; },
        [](int a, int b) { return a * b; }
    };
    auto fn_ref1 = alib6::ref(fn_vec, 0);
    auto fn_ref2 = alib6::ref(fn_vec, 1);
    EXPECT_EQ(fn_ref1(3, 4), 7);
    EXPECT_EQ(fn_ref2(3, 4), 12);

    // 3. 算术与比较运算符穿透
    std::vector<int> num_vec = {100};
    auto num_ref = alib6::ref(num_vec, 0);
    num_ref += 50;
    EXPECT_EQ(num_vec[0], 150);
    EXPECT_TRUE(num_ref > 100);
    EXPECT_FALSE(num_ref < 100);
    EXPECT_EQ(num_ref + 10, 160);
}

TEST(RefTest, MultiRefBasicAndNavigation) {
    std::vector<std::vector<int>> matrix = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    auto mr = alib6::refs(matrix, 1, 2);
    EXPECT_EQ(*mr, 6);

    *mr = 600;
    EXPECT_EQ(matrix[1][2], 600);

    // 显式层级与索引导航
    auto parent = mr.parent();
    EXPECT_EQ(parent->size(), 3);

    auto child = parent.child(0);
    EXPECT_EQ(*child, 4);

    auto neighbor = mr.with_index(1);
    EXPECT_EQ(*neighbor, 5);

    mr.prev_element();
    EXPECT_EQ(*mr, 5);
}
