/**
 * @file test_adata.cpp
 * @brief alib6 动态数据结构 AData / Value / JSON / TOML / Validator 单元测试与 PMR 泄漏检测
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
#include <meta>
import std;
import alib6;

using namespace alib6;
using namespace alib6::data;

TEST(ADataTest, ValueBasicAndConversions) {
    alib6::test::CountingMemoryResource tracker;

    {
        Value v_int(12345, &tracker);
        EXPECT_EQ(v_int.get_type(), Value::INT);
        EXPECT_EQ(v_int.to<i64>(), 12345);
        EXPECT_EQ(v_int.to<int>(), 12345);
        EXPECT_EQ(v_int.to<std::string_view>(), "12345");

        Value v_double(3.14159, &tracker);
        EXPECT_EQ(v_double.get_type(), Value::FLOATING);
        EXPECT_NEAR(v_double.to<double>(), 3.14159, 1e-5);

        Value v_bool(true, &tracker);
        EXPECT_EQ(v_bool.get_type(), Value::BOOL);
        EXPECT_TRUE(v_bool.to<bool>());
        EXPECT_EQ(v_bool.to<std::string_view>(), "true");

        Value v_str("hello_alib6", &tracker);
        EXPECT_EQ(v_str.get_type(), Value::STRING);
        EXPECT_EQ(v_str.to<std::string_view>(), "hello_alib6");

        // 测试 transform
        Value v_transform("12345", &tracker);
        v_transform.transform<int>() = 999;
        EXPECT_EQ(v_transform.get_type(), Value::INT);
        EXPECT_EQ(v_transform.to<int>(), 999);

        // 测试 reconstruct
        Value v_reconst("hello_alib6", &tracker);
        v_reconst.reconstruct<int>() = 888;
        EXPECT_EQ(v_reconst.get_type(), Value::INT);
        EXPECT_EQ(v_reconst.to<int>(), 888);

        // 测试 expect
        Value v_num_str("45678", &tracker);
        auto exp_int = v_num_str.expect<int>();
        EXPECT_TRUE(exp_int.second);
        EXPECT_EQ(exp_int.first, 45678);

        Value v_invalid("abc", &tracker);
        auto exp_invalid = v_invalid.expect<int>();
        EXPECT_FALSE(exp_invalid.second);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, ValueCompareStrategy) {
    alib6::test::CountingMemoryResource tracker;

    {
        Value v1(100, &tracker);
        Value v2(100.0, &tracker);
        Value v3("100", &tracker);
        Value v4(true, &tracker);
        Value v5(1, &tracker);

        // Strict: 类型必须严格一致
        EXPECT_FALSE(v1.equals(v2, CompareStrategy::Strict));
        EXPECT_FALSE(v1.equals(v3, CompareStrategy::Strict));

        // BoolStrict: INT 与 DOUBLE 可比
        EXPECT_TRUE(v1.equals(v2, CompareStrategy::BoolStrict));
        EXPECT_FALSE(v1.equals(v3, CompareStrategy::BoolStrict));

        // Lesser: 支持 bool 隐式比对
        EXPECT_TRUE(v4.equals(v5, CompareStrategy::Lesser));

        // Fuzzy: 字符串数字与整型跨类型模糊比对
        EXPECT_TRUE(v1.equals(v3, CompareStrategy::Fuzzy));
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, ADataTreeOperations) {
    alib6::test::CountingMemoryResource tracker;

    {
        AData root(&tracker);
        root["name"] = "Alice";
        root["age"] = 30;
        root["scores"][0] = 95;
        root["scores"][1] = 88;
        root["scores"][2] = 100;

        EXPECT_TRUE(root.is_object());
        EXPECT_TRUE(root["scores"].is_array());
        EXPECT_EQ(root["name"].to<std::string_view>(), "Alice");
        EXPECT_EQ(root["age"].to<int>(), 30);
        EXPECT_EQ(root["scores"].array().size(), 3);
        EXPECT_EQ(root["scores"][0].to<int>(), 95);
        EXPECT_EQ(root["scores"][-1].to<int>(), 100); // 负索引支持

        // Object 操作 (contains, rename, remove)
        auto& obj = root.object();
        EXPECT_TRUE(obj.contains("name"));
        EXPECT_TRUE(obj.rename("name", "nickname"));
        EXPECT_FALSE(obj.contains("name"));
        EXPECT_TRUE(obj.contains("nickname"));
        EXPECT_EQ(root["nickname"].to<std::string_view>(), "Alice");

        EXPECT_TRUE(obj.remove("age"));
        EXPECT_FALSE(obj.contains("age"));
        EXPECT_EQ(obj.size(), 2); // nickname, scores

        // Object 迭代
        usize iter_count = 0;
        for (auto proxy : root.object()) {
            EXPECT_FALSE(proxy.first().empty());
            ++iter_count;
        }
        EXPECT_EQ(iter_count, 2);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, JSONPointerJump) {
    alib6::test::CountingMemoryResource tracker;

    {
        AData root(&tracker);
        root["user"]["profile"]["name"] = "Bob";
        root["user"]["tags"][0] = "admin";
        root["user"]["tags"][1] = "developer";
        root["special/key~name"] = "escaped_val";

        // 普通路径
        EXPECT_EQ(root.jump("/user/profile/name").to<std::string_view>(), "Bob");
        EXPECT_EQ(root.jump("/user/tags/1").to<std::string_view>(), "developer");

        // RFC 6901 转义路径 (~0 -> ~, ~1 -> /)
        EXPECT_EQ(root.jump("/special~1key~0name").to<std::string_view>(), "escaped_val");

        // 指针模式与 nullptr 检查
        EXPECT_EQ(root.jump_ptr("/not_exist", false), nullptr);
        EXPECT_EQ(root.jump_ptr("/user/tags/99", false), nullptr);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, MergeDiffPruneMigrate) {
    alib6::test::CountingMemoryResource tracker;

    {
        // 1. Merge
        AData dest(&tracker);
        dest["a"] = 1;
        dest["b"]["c"] = 2;

        AData src(&tracker);
        src["b"]["d"] = 3;
        src["e"] = 4;

        dest.merge(src);
        EXPECT_EQ(dest["a"].to<int>(), 1);
        EXPECT_EQ(dest["b"]["c"].to<int>(), 2);
        EXPECT_EQ(dest["b"]["d"].to<int>(), 3);
        EXPECT_EQ(dest["e"].to<int>(), 4);

        // 2. Diff
        AData original(&tracker);
        original["x"] = 10;
        original["y"] = 20;

        AData modified(&tracker);
        modified["x"] = 10;
        modified["y"] = 25; // modified
        modified["z"] = 30; // added

        AData added_mod(&tracker);
        AData lack(&tracker);
        bool has_diff = original.diff(modified, &added_mod, &lack);
        EXPECT_TRUE(has_diff);
        EXPECT_EQ(added_mod["y"].to<int>(), 25);
        EXPECT_EQ(added_mod["z"].to<int>(), 30);

        // 3. Prune
        AData tree(&tracker);
        tree["valid"] = 123;
        tree["empty_obj"].set<AData::Object>();
        tree["null_val"].set_null();

        tree.prune();
        EXPECT_TRUE(tree.object().contains("valid"));
        EXPECT_FALSE(tree.object().contains("empty_obj"));
        EXPECT_FALSE(tree.object().contains("null_val"));

        // 4. Migrate
        AData migrated = migrate(dest, &tracker);
        EXPECT_EQ(migrated["a"].to<int>(), 1);
        EXPECT_EQ(migrated["b"]["d"].to<int>(), 3);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, SelfReferencingMoveAndCopy) {
    alib6::test::CountingMemoryResource tracker;

    {
        // 1. data = data["sub"] (复制子对象到自身)
        {
            AData data(&tracker);
            data["sub"]["value"] = 42;
            data["sub"]["tag"] = "test";
            data["other"] = "dummy";

            data = data["sub"];
            EXPECT_TRUE(data.is_object());
            EXPECT_EQ(data["value"].to<int>(), 42);
            EXPECT_EQ(data["tag"].to<std::string_view>(), "test");
            EXPECT_FALSE(data.object().contains("other"));
        }

        // 2. data = std::move(data["sub"]) (移动子对象到自身)
        {
            AData data(&tracker);
            data["sub"]["name"] = "Alice";
            data["sub"]["score"] = 99;
            data["sub"]["items"][0] = "item1";
            data["extra"] = 12345;

            data = std::move(data["sub"]);
            EXPECT_TRUE(data.is_object());
            EXPECT_EQ(data["name"].to<std::string_view>(), "Alice");
            EXPECT_EQ(data["score"].to<int>(), 99);
            EXPECT_EQ(data["items"][0].to<std::string_view>(), "item1");
            EXPECT_FALSE(data.object().contains("extra"));
        }

        // 3. 多层深层子节点移动到根节点: data = std::move(data["deep"]["inner"])
        {
            AData data(&tracker);
            data["deep"]["inner"]["x"] = 10;
            data["deep"]["inner"]["y"] = 20;
            data["other"] = 999;

            data = std::move(data["deep"]["inner"]);
            EXPECT_TRUE(data.is_object());
            EXPECT_EQ(data["x"].to<int>(), 10);
            EXPECT_EQ(data["y"].to<int>(), 20);
            EXPECT_FALSE(data.object().contains("deep"));
            EXPECT_FALSE(data.object().contains("other"));
        }

        // 4. 多层深层子节点拷贝到根节点: data = data["deep"]["inner"]
        {
            AData data(&tracker);
            data["deep"]["inner"]["name"] = "DeepConfig";
            data["extra"] = true;

            data = data["deep"]["inner"];
            EXPECT_TRUE(data.is_object());
            EXPECT_EQ(data["name"].to<std::string_view>(), "DeepConfig");
            EXPECT_FALSE(data.object().contains("deep"));
        }

        // 5. 数组子项移动到根节点: data = std::move(data[0])
        {
            AData data(&tracker);
            data[0]["msg"] = "hello";
            data[1]["msg"] = "world";

            data = std::move(data[0]);
            EXPECT_TRUE(data.is_object());
            EXPECT_EQ(data["msg"].to<std::string_view>(), "hello");
        }

        // 6. 数组子项拷贝到根节点: data = data[1]
        {
            AData data(&tracker);
            data[0] = 100;
            data[1] = 200;

            data = data[1];
            EXPECT_TRUE(data.is_value());
            EXPECT_EQ(data.to<int>(), 200);
        }
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, JSONPolicySerialization) {
    alib6::test::CountingMemoryResource tracker;

    {
        std::string_view json_text = R"({
            "name": "Antigravity",
            "version": 6,
            "features": ["pmr", "modules", "simd"],
            "nested": {
                "active": true,
                "pi": 3.14159
            }
        })";

        AData doc(&tracker);
        JSON json_policy;
        bool ok = json_policy.parse(json_text, doc);
        EXPECT_TRUE(ok);
        EXPECT_EQ(doc["name"].to<std::string_view>(), "Antigravity");
        EXPECT_EQ(doc["version"].to<int>(), 6);
        EXPECT_EQ(doc["features"].array().size(), 3);
        EXPECT_TRUE(doc["nested"]["active"].to<bool>());
        EXPECT_NEAR(doc["nested"]["pi"].to<double>(), 3.14159, 1e-5);

        // Dump 测试 (带排序与紧凑排版)
        JSONConfig cfg;
        cfg.compact_lines = true;
        cfg.compact_spaces = true;
        cfg.sort_object = JSONConfig::sort_asc;

        JSON compact_json(cfg);
        auto dumped = doc.dump_to_string(compact_json, &tracker);
        EXPECT_FALSE(dumped.empty());

        // 重新解析转储后的 JSON 验证正确性
        AData doc2(&tracker);
        EXPECT_TRUE(compact_json.parse(dumped, doc2));
        EXPECT_TRUE(doc.equals(doc2));
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, TOMLPolicySerialization) {
    alib6::test::CountingMemoryResource tracker;

    {
        std::string_view toml_text = R"(
            title = "alib6 TOML Test"
            port = 8080
            enabled = true
            ratio = 1.618

            [server]
            host = "127.0.0.1"
            workers = 8

            [clients]
            ips = [ "192.168.1.1", "192.168.1.2" ]
        )";

        AData doc(&tracker);
        TOML toml_policy;
        bool ok = toml_policy.parse(toml_text, doc);
        EXPECT_TRUE(ok);
        EXPECT_EQ(doc["title"].to<std::string_view>(), "alib6 TOML Test");
        EXPECT_EQ(doc["port"].to<int>(), 8080);
        EXPECT_TRUE(doc["enabled"].to<bool>());
        EXPECT_NEAR(doc["ratio"].to<double>(), 1.618, 1e-3);
        EXPECT_EQ(doc["server"]["host"].to<std::string_view>(), "127.0.0.1");
        EXPECT_EQ(doc["server"]["workers"].to<int>(), 8);
        EXPECT_EQ(doc["clients"]["ips"].array().size(), 2);

        // TOML 转储测试
        auto dumped_toml = doc.dump_to_string(toml_policy, &tracker);
        EXPECT_FALSE(dumped_toml.empty());

        // 重新解析转储结果验证往返完整性
        AData doc_reparsed(&tracker);
        EXPECT_TRUE(toml_policy.parse(dumped_toml, doc_reparsed));
        EXPECT_EQ(doc_reparsed["title"].to<std::string_view>(), "alib6 TOML Test");
        EXPECT_EQ(doc_reparsed["server"]["workers"].to<int>(), 8);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, ValidatorSchema) {
    alib6::test::CountingMemoryResource tracker;

    {
        AData schema(&tracker);
        schema["username"] = "TYPE STRING MIN 3 MAX 16 REQUIRED";
        schema["age"] = "TYPE INT MIN 0 MAX 150 OPTIONAL";
        schema["role"] = "ENUM ( Admin User Guest )";
        schema["preferences"]["theme"][0] = "TYPE STRING";
        schema["preferences"]["theme"][1] = "dark"; // 默认值

        Validator validator(schema, &tracker);

        // 1. 合法文档校验
        AData valid_doc(&tracker);
        valid_doc["username"] = "dev_user";
        valid_doc["age"] = 25;
        valid_doc["role"] = "Admin";

        auto r1 = validator.validate(valid_doc);
        EXPECT_TRUE(r1.success);
        EXPECT_TRUE(valid_doc["preferences"].is_object());
        EXPECT_EQ(valid_doc["preferences"]["theme"].to<std::string_view>(), "dark"); // 自动注入默认值

        // 2. 非法文档 (用户名太短、枚举不匹配)
        AData invalid_doc(&tracker);
        invalid_doc["username"] = "ab"; // 长度 < 3
        invalid_doc["role"] = "SuperHacker"; // 非法枚举

        auto r2 = validator.validate(invalid_doc);
        EXPECT_FALSE(r2.success);
        EXPECT_GE(r2.recorded_errors.size(), 2);

        // 3. 自定义校验器 Hook
        AData schema_with_hook(&tracker);
        schema_with_hook["port"] = "VALIDATE is_valid_port";
        Validator v_hook(schema_with_hook, &tracker);
        v_hook.emplace_validate_method("is_valid_port", [](AData& n, auto&) {
            int p = n.to<int>();
            return p >= 1024 && p <= 65535;
        });

        AData doc_port_ok(&tracker);
        doc_port_ok["port"] = 8080;
        EXPECT_TRUE(v_hook.validate(doc_port_ok).success);

        AData doc_port_bad(&tracker);
        doc_port_bad["port"] = 80;
        EXPECT_FALSE(v_hook.validate(doc_port_bad).success);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        AData root(&tracker);
        root["service"] = "auth";
        root["replicas"] = 5;
        for (int i = 0; i < 10; ++i) {
            root["endpoints"][i] = std::format("10.0.0.{}", i);
        }

        JSON json_policy;
        auto json_str = root.dump_to_string(json_policy, &tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);

        AData doc2(&tracker);
        json_policy.parse(json_str, doc2);
        EXPECT_TRUE(root.equals(doc2));

        TOML toml_policy;
        auto toml_str = root.dump_to_string(toml_policy, &tracker);
        AData doc3(&tracker);
        toml_policy.parse(toml_str, doc3);
        EXPECT_EQ(doc3["service"].to<std::string_view>(), "auth");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

// ==================== 14. 严格全局 operator new 劫持与零全局堆分配检测 ====================
TEST(ADataTest, ZeroGlobalHeapAllocations) {
    alib6::test::CountingMemoryResource tracker;

    std::size_t global_allocs = 0;
    bool parse_ok = false;
    bool equals_ok = false;
    bool has_diff = false;
    bool dump_empty = true;

    {
        alib6::test::HeapAllocationGuard guard;

        AData root(&tracker);
        root["service"] = "AuthCluster";
        root["port"] = 8080;
        root["active"] = true;
        root["metrics"]["limits"]["cpu"] = 4.5;
        root["metrics"]["limits"]["memory"] = 16384;
        root["workers"][0] = "Worker-0";
        root["workers"][1] = "Worker-1";

        // JSON 解析与转储
        JSON json_policy;
        auto json_str = root.dump_to_string(json_policy, &tracker);
        dump_empty = json_str.empty();

        AData parsed(&tracker);
        parse_ok = parsed.load_from_memory(json_str, json_policy);
        equals_ok = root.equals(parsed);

        // Merge & Diff & Clean
        AData extra(&tracker);
        extra["metrics"]["limits"]["gpu"] = 2;
        extra["service"] = "AuthClusterV2";
        root.merge(extra);

        AData added(&tracker);
        AData lack(&tracker);
        has_diff = root.diff(parsed, &added, &lack);

        global_allocs = guard.new_allocations();
    }

    EXPECT_FALSE(dump_empty);
    EXPECT_TRUE(parse_ok);
    EXPECT_TRUE(equals_ok);
    EXPECT_TRUE(has_diff);

    // 验证整个操作生命周期内全局 operator new 分配次数必须为 0
    EXPECT_EQ(global_allocs, 0);

    // 作用域析构后 PMR 内存 100% 归还
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

// ==================== 15. Flat 扁平化控制台输出策略 ====================
namespace {

    struct FlatWindow {
        u32 x{0};
        u32 y{0};
    };

    struct FlatOuter {
        int id{0};
        std::string name;
        bool active{false};
        double ratio{0.0};
        std::vector<int> tags;
        FlatWindow win{};
        std::optional<std::string> note{std::nullopt};
    };

} // namespace

TEST(ADataTest, FlatPolicyDump) {
    alib6::test::CountingMemoryResource tracker;

    {
        FlatWindow window{10, 20};
        EXPECT_EQ(to_adata(window).str<Flat>(), "x=10,y=20");

        FlatOuter outer;
        outer.id = 7;
        outer.name = "123"; // 形似数字, 应被加引号避免解析歧义
        outer.active = true;
        outer.ratio = 1.5;
        outer.tags = {1, 2, 3};
        outer.win = {10, 20};

        auto ad = to_adata(outer, &tracker);
        auto text = ad.str<Flat>();
        EXPECT_EQ(
            text,
            "active=true,id=7,name=\"123\",note=null,ratio=1.5,"
            "tags[0]=1,tags[1]=2,tags[2]=3,win.x=10,win.y=20"
        );

        // 自定义配置: 点号数组下标 + 保留插入 (排序关闭) 亦可工作
        FlatConfig cfg;
        cfg.array_bracket = false;
        Flat flat_custom(cfg);
        auto text2 = ad.str(flat_custom);
        EXPECT_NE(text2.find("tags.0=1"), std::string::npos);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, FlatPolicyLossyParseRoundTrip) {
    alib6::test::CountingMemoryResource tracker;

    {
        FlatWindow window{10, 20};
        auto text = to_adata(window).str<Flat>();
        EXPECT_EQ(text, "x=10,y=20");

        // 有损解析回 AData: 数字可还原类型, 嵌套路径可还原结构
        AData doc(&tracker);
        Flat flat;
        EXPECT_TRUE(flat.parse(text, doc));
        EXPECT_TRUE(doc.is_object());
        EXPECT_EQ(doc["x"].to<int>(), 10);
        EXPECT_EQ(doc["y"].to<int>(), 20);

        // 复杂结构的往返 (类型保真的部分)
        FlatOuter outer;
        outer.id = 7;
        outer.name = "123"; // 带引号, 往返后仍是字符串
        outer.active = true;
        outer.ratio = 1.5;
        outer.tags = {1, 2, 3};
        outer.win = {10, 20};

        auto src = to_adata(outer, &tracker);
        auto dumped = src.str<Flat>();

        AData back(&tracker);
        EXPECT_TRUE(flat.parse(dumped, back));
        EXPECT_EQ(back["id"].to<int>(), 7);
        EXPECT_EQ(back["name"].to<std::string_view>(), "123");
        EXPECT_TRUE(back["active"].to<bool>());
        EXPECT_NEAR(back["ratio"].to<double>(), 1.5, 1e-9);
        EXPECT_TRUE(back["note"].is_null());
        EXPECT_EQ(back["win"]["x"].to<int>(), 10);
        EXPECT_EQ(back["win"]["y"].to<int>(), 20);
        EXPECT_EQ(back["tags"].array().size(), 3);
        EXPECT_EQ(back["tags"][1].to<int>(), 2);

        // 有损性验证: 无引号的形似数字字符串会被推断成整数
        AData lossy(&tracker);
        EXPECT_TRUE(flat.parse("name=123", lossy));
        EXPECT_EQ(lossy["name"].value().get_type(), Value::INT);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ADataTest, FlatPolicyPMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        FlatWindow window{1, 2};
        auto ad = to_adata(window, &tracker);

        auto text = ad.str<Flat>();
        EXPECT_EQ(text, "x=1,y=2");
        EXPECT_GT(tracker.allocated_bytes(), 0);

        AData parsed(&tracker);
        Flat flat;
        EXPECT_TRUE(flat.parse(text, parsed));
        EXPECT_EQ(parsed["x"].to<int>(), 1);
        EXPECT_EQ(parsed["y"].to<int>(), 2);
    }

    // 作用域析构后, 内存必须 100% 归还, 绝无泄漏
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
