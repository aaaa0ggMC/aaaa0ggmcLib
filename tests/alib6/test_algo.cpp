#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>

import alib6;

TEST(AlgoTest, SortingAlgorithmsAscendingAndDescending) {
    const std::vector<int> original = {64, 34, 25, 12, 22, 11, 90, -5, 0, 42};
    std::vector<int> expected_asc = original;
    std::sort(expected_asc.begin(), expected_asc.end());
    std::vector<int> expected_desc = original;
    std::sort(expected_desc.begin(), expected_desc.end(), std::greater<>{});

    // 1. Bubble Sort
    {
        auto v = original;
        alib6::algo::bubble_sort(v);
        EXPECT_EQ(v, expected_asc);

        alib6::algo::bubble_sort(v, std::less<>{}, true);
        EXPECT_EQ(v, expected_desc);
    }

    // 2. Cocktail Sort
    {
        auto v = original;
        alib6::algo::cocktail_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 3. Comb Sort
    {
        auto v = original;
        alib6::algo::comb_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 4. Gnome Sort
    {
        auto v = original;
        alib6::algo::gnome_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 5. Insertion Sort
    {
        auto v = original;
        alib6::algo::insertion_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 6. Odd-Even Sort
    {
        auto v = original;
        alib6::algo::odd_even_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 7. Selection Sort
    {
        auto v = original;
        alib6::algo::selection_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 8. Shell Sort
    {
        auto v = original;
        alib6::algo::shell_sort(v);
        EXPECT_EQ(v, expected_asc);
    }

    // 9. Quick Sort
    {
        auto v = original;
        alib6::algo::quick_sort(v);
        EXPECT_EQ(v, expected_asc);

        alib6::algo::quick_sort(v, std::less<>{}, true);
        EXPECT_EQ(v, expected_desc);
    }
}

TEST(AlgoTest, SwapInjectionTracking) {
    std::vector<int> v = {5, 4, 3, 2, 1};
    int swap_count = 0;

    alib6::algo::bubble_sort(v, std::less<>{}, false, [&](std::size_t i, std::size_t j) {
        ++swap_count;
        EXPECT_NE(i, j);
    });

    EXPECT_GT(swap_count, 0);
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

TEST(AlgoTest, SearchingAlgorithmsPlainAndKMP) {
    std::string text = "ABC ABCDAB ABCDABCDABDE";
    std::string pattern = "ABCDABD";

    // 1. Plain Search
    std::size_t pos_plain = alib6::algo::plain_search(text, pattern);
    EXPECT_EQ(pos_plain, 15);

    // 2. KMP Search
    std::size_t pos_kmp = alib6::algo::kmp_search(text, pattern);
    EXPECT_EQ(pos_kmp, 15);

    // 3. Not found
    EXPECT_EQ(alib6::algo::kmp_search(text, "NOT_IN_TEXT"), std::string_view::npos);
}
