#include <gtest/gtest.h>
#include "pmr_tracker.h"
#include <string_view>
#include <string>

import alib6;

namespace {

    constexpr std::string_view en_us_json = R"({
        "id": "en_us",
        "title": "English",
        "app": {
            "name": "GameStudio",
            "welcome": "Welcome back, {}!",
            "score": "Player {} scored {} in {}",
            "nested": {
                "desc": "Deep translation"
            }
        },
        "items": [
            "Sword",
            "Shield",
            "Potion"
        ]
    })";

    constexpr std::string_view zh_cn_json = R"({
        "id": "zh_cn",
        "title": "简体中文",
        "app": {
            "name": "游戏工作室",
            "welcome": "欢迎回来，{}！",
            "score": "玩家 {} 在 {} 获得了 {} 分",
            "nested": {
                "desc": "深度翻译"
            }
        },
        "items": [
            "宝剑",
            "盾牌",
            "药水"
        ]
    })";

} // namespace

TEST(TranslatorTest, BasicTranslationAndFormatting) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Translator tr(&tracker);
        bool loaded = tr.load_from_memory(en_us_json);
        EXPECT_TRUE(loaded);
        EXPECT_EQ(tr.current_language, "en_us");

        // 1. JSON 指针直接查询
        EXPECT_EQ(tr.get_key_value("/app/name"), "GameStudio");
        EXPECT_EQ(tr.get_key_value("/app/nested/desc"), "Deep translation");
        EXPECT_EQ(tr.get_key_value("/items/0"), "Sword");
        EXPECT_EQ(tr.get_key_value("/items/2"), "Potion");

        // 2. 点分格式查询
        EXPECT_EQ(tr.get_key_value_dots("app.name"), "GameStudio");
        EXPECT_EQ(tr.get_key_value_dots("app.nested.desc"), "Deep translation");
        EXPECT_EQ(tr.get_key_value_dots("items.1"), "Shield");

        // 3. 不存在 key 回退自身
        EXPECT_EQ(tr.get_key_value("/app/non_existent"), "/app/non_existent");
        EXPECT_EQ(tr.get_key_value_dots("app.unknown.key"), "app.unknown.key");

        // 4. 格式化翻译 (translate)
        auto msg1 = tr.translate("/app/welcome", "Alice");
        EXPECT_EQ(msg1, "Welcome back, Alice!");

        auto msg2 = tr.translate("/app/score", "Bob", 9500, "Zone 3");
        EXPECT_EQ(msg2, "Player Bob scored 9500 in Zone 3");

        // 5. 零堆分配目标追加 (translate_to)
        std::string target_buf = "HEADER: ";
        tr.translate_to("/app/welcome", target_buf, "Charlie");
        EXPECT_EQ(target_buf, "HEADER: Welcome back, Charlie!");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(TranslatorTest, MultiLanguageSwitching) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Translator tr(&tracker);
        EXPECT_TRUE(tr.load_from_memory(en_us_json));
        EXPECT_TRUE(tr.load_from_memory(zh_cn_json));

        EXPECT_EQ(tr.current_language, "en_us");
        EXPECT_EQ(tr.get_key_value("/app/name"), "GameStudio");

        // 切换语言为中文
        EXPECT_TRUE(tr.switch_language("zh_cn"));
        EXPECT_EQ(tr.current_language, "zh_cn");
        EXPECT_EQ(tr.get_key_value("/app/name"), "游戏工作室");
        EXPECT_EQ(tr.get_key_value("/items/0"), "宝剑");

        auto zh_msg = tr.translate("/app/welcome", "小明");
        EXPECT_EQ(zh_msg, "欢迎回来，小明！");

        // 切换不存在的语言返回 false，保持原语言
        EXPECT_FALSE(tr.switch_language("fr_fr"));
        EXPECT_EQ(tr.current_language, "zh_cn");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(TranslatorTest, FlattenTranslatorDotsAndJsonP) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Translator tr(&tracker);
        EXPECT_TRUE(tr.load_from_memory(en_us_json));
        EXPECT_TRUE(tr.load_from_memory(zh_cn_json));

        // 1. 点分格式平铺 (Dots)
        auto flat_dots_opt = tr.flatten_dots("zh_cn", 1024, &tracker);
        ASSERT_TRUE(flat_dots_opt.has_value());
        auto flat_dots = std::move(*flat_dots_opt);

        EXPECT_EQ(flat_dots.type, alib6::FlattenTranslator::Type::Dots);
        EXPECT_GT(flat_dots.size(), 0);
        EXPECT_EQ(flat_dots.get_key_value("app.name"), "游戏工作室");
        EXPECT_EQ(flat_dots.get_key_value("app.nested.desc"), "深度翻译");
        EXPECT_EQ(flat_dots.get_key_value("items.0"), "宝剑");
        EXPECT_EQ(flat_dots.get_key_value("items.2"), "药水");

        // 2. JSON 指针格式平铺 (JsonP)
        auto flat_jsonp_opt = tr.flatten_jsonp("en_us", 1024, &tracker);
        ASSERT_TRUE(flat_jsonp_opt.has_value());
        auto flat_jsonp = std::move(*flat_jsonp_opt);

        EXPECT_EQ(flat_jsonp.type, alib6::FlattenTranslator::Type::JsonP);
        EXPECT_EQ(flat_jsonp.get_key_value("/app/name"), "GameStudio");
        EXPECT_EQ(flat_jsonp.get_key_value("/items/1"), "Shield");

        // 3. 平铺翻译器格式化与修改器
        auto formatted = flat_dots.translate("app.welcome", "李四");
        EXPECT_EQ(formatted, "欢迎回来，李四！");

        // 4. 拷贝与移动构造测试
        alib6::FlattenTranslator copied_flat = flat_dots;
        EXPECT_EQ(copied_flat.get_key_value("app.name"), "游戏工作室");

        alib6::FlattenTranslator moved_flat = std::move(copied_flat);
        EXPECT_EQ(moved_flat.get_key_value("app.name"), "游戏工作室");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(TranslatorTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::Translator tr(&tracker);
        EXPECT_TRUE(tr.load_from_memory(en_us_json));
        EXPECT_TRUE(tr.load_from_memory(zh_cn_json));

        EXPECT_GT(tracker.allocated_bytes(), 0);

        auto flat = tr.flatten_dots("zh_cn", 512, &tracker);
        ASSERT_TRUE(flat.has_value());

        auto str = flat->translate("app.welcome", "PMR Test");
        EXPECT_GT(tracker.allocated_bytes(), 0);
    }

    // 作用域析构后，内存必须 100% 归还，无泄漏
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
