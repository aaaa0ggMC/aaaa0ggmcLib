#include <gtest/gtest.h>
#include "pmr_tracker.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <meta>
#include <print>
import alib6;

namespace {

    enum class ServerRole {
        Master,
        Worker,
        Gateway,
    };

    struct SubConfig {
        std::string ip{"127.0.0.1"};
        int port{8080};
    };

    struct UserProfile {
        [[=alib6::attr::rename<"user_id">{}]]
        int id{1001};

        std::string username{"admin"};

        ServerRole role{ServerRole::Master};

        [[=alib6::attr::seri::omit_empty{}]]
        std::string nickname{""};

        [[=alib6::attr::seri::skip{}]]
        int runtime_token{9999};

        [[=alib6::attr::deseri::skip{}]]
        std::string computed_hash{"initial_hash"};

        [[=alib6::attr::skip{}]]
        int ignored_field{42};

        std::optional<std::string> email{std::nullopt};

        std::vector<int> permissions{1, 2, 3};

        SubConfig endpoint{};
    };

    struct ADataEnvelope {
        alib6::AData content;
    };

    struct [[=alib6::attr::schema::extra<"MIN_KEYS 1">{}]] ServerCluster {
        std::string cluster_name{"APAC-1"};
        [[=alib6::attr::schema::range{1.0, 65535.0}]]
        int base_port{8000};
        std::map<std::string, SubConfig> nodes;
    };

    //// 键级约束边界测试: 结构体成员上的 optional / required 互不传染 ////
    struct WindowConfig {
        [[=alib6::attr::schema::required{}]]
        alib6::u32 width{800};
        alib6::u32 height{600};
    };

    struct FogAppConfig {
        [[=alib6::attr::schema::optional{}]]
        WindowConfig window;
        alib6::u32 max_steps{64};
    };

    struct OptionalWindowHolder {
        [[=alib6::attr::schema::optional{}]]
        std::optional<WindowConfig> window;
        int mode{1};
    };

    //// default_value 通过 optional 壳落地 ////
    inline constexpr alib6::u32 kWindowDefaultWidth = 10;
    inline constexpr alib6::u32 kWindowDefaultHeight = 100;
    inline constexpr int kGadgetDefaultLevel = 42;

    struct Gadget {
        [[=alib6::attr::schema::default_value{kGadgetDefaultLevel}]]
        int level{42};
    };

    struct DeepSub {
        [[=alib6::attr::schema::required{}]]
        Gadget gadget;
        int plain{3};
    };

    struct NestedHolder {
        [[=alib6::attr::schema::optional{}]]
        DeepSub sub;
        int mode{1};
    };

    struct WindowWithDefaults {
        [[=alib6::attr::schema::default_value{kWindowDefaultWidth}]]
        alib6::u32 width{kWindowDefaultWidth};
        [[=alib6::attr::schema::default_value{kWindowDefaultHeight}]]
        alib6::u32 height{kWindowDefaultHeight};
    };

    struct DefaultsHolder {
        [[=alib6::attr::schema::optional{}]]
        WindowWithDefaults window;
        int mode{1};
    };

} // namespace

TEST(ReflectTest, BasicStructSerializationAndDeserialization) {
    alib6::test::CountingMemoryResource tracker;

    {
        UserProfile profile;
        profile.id = 777;
        profile.username = "alice";
        profile.role = ServerRole::Gateway;
        profile.nickname = ""; // Should be omitted by omit_empty
        profile.runtime_token = 8888;
        profile.computed_hash = "my_hash";
        profile.ignored_field = 1234;
        profile.email = "alice@example.com";
        profile.permissions = {10, 20, 30};
        profile.endpoint.ip = "192.168.1.100";
        profile.endpoint.port = 9000;

        // 1. to_adata
        auto doc = alib6::to_adata(profile, &tracker);
        EXPECT_TRUE(doc.is_object());

        // 验证 rename
        EXPECT_TRUE(doc.object().contains("user_id"));
        EXPECT_EQ(doc["user_id"].to<int>(), 777);
        EXPECT_FALSE(doc.object().contains("id"));

        // 验证基本字段与 enum
        EXPECT_EQ(doc["username"].to<std::string_view>(), "alice");
        EXPECT_EQ(doc["role"].to<std::string_view>(), "Gateway");

        // 验证 omit_empty
        EXPECT_FALSE(doc.object().contains("nickname"));

        // 验证 seri::skip 与 general skip
        EXPECT_FALSE(doc.object().contains("runtime_token"));
        EXPECT_FALSE(doc.object().contains("ignored_field"));

        // 验证 optional 与 vector
        EXPECT_EQ(doc["email"].to<std::string_view>(), "alice@example.com");
        EXPECT_EQ(doc["permissions"].array().size(), 3);
        EXPECT_EQ(doc["permissions"][1].to<int>(), 20);

        // 验证 nested struct
        EXPECT_EQ(doc["endpoint"]["ip"].to<std::string_view>(), "192.168.1.100");
        EXPECT_EQ(doc["endpoint"]["port"].to<int>(), 9000);

        // 2. from_adata
        UserProfile restored;
        restored.computed_hash = "preserved_hash";
        bool success = alib6::from_adata(restored, doc);
        EXPECT_TRUE(success);

        EXPECT_EQ(restored.id, 777);
        EXPECT_EQ(restored.username, "alice");
        EXPECT_EQ(restored.role, ServerRole::Gateway);
        EXPECT_TRUE(restored.email.has_value());
        EXPECT_EQ(*restored.email, "alice@example.com");
        EXPECT_EQ(restored.permissions.size(), 3);
        EXPECT_EQ(restored.permissions[2], 30);
        EXPECT_EQ(restored.endpoint.ip, "192.168.1.100");
        EXPECT_EQ(restored.endpoint.port, 9000);

        // 验证 deseri::skip 未被覆盖
        EXPECT_EQ(restored.computed_hash, "preserved_hash");
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, ADataPassThroughAndNestedMember) {
    alib6::test::CountingMemoryResource source_memory;
    alib6::test::CountingMemoryResource destination_memory;

    {
        alib6::AData source(&source_memory);
        source["name"] = "debug_messenger";
        source["succeeded"] = true;

        auto copied = alib6::to_adata(source, &destination_memory);
        EXPECT_EQ(copied.get_allocator(), &destination_memory);
        EXPECT_EQ(copied["name"].to<std::string_view>(), "debug_messenger");
        EXPECT_TRUE(copied["succeeded"].to<bool>());

        alib6::AData restored(&destination_memory);
        EXPECT_TRUE(alib6::from_adata(restored, source));
        EXPECT_EQ(restored.get_allocator(), &destination_memory);
        EXPECT_EQ(restored["name"].to<std::string_view>(), "debug_messenger");

        ADataEnvelope envelope;
        envelope.content["stage"] = "create_instance";
        envelope.content["vk_result"] = 0;

        auto document = alib6::to_adata(envelope, &destination_memory);
        EXPECT_EQ(
            document["content"]["stage"].to<std::string_view>(),
            "create_instance"
        );
        EXPECT_NE(document.str().find("create_instance"), std::string_view::npos);

        ADataEnvelope restored_envelope;
        EXPECT_TRUE(alib6::from_adata(restored_envelope, document));
        EXPECT_EQ(
            restored_envelope.content["stage"].to<std::string_view>(),
            "create_instance"
        );
        EXPECT_EQ(restored_envelope.content["vk_result"].to<int>(), 0);
    }

    EXPECT_FALSE(source_memory.has_leak());
    EXPECT_FALSE(destination_memory.has_leak());
}

TEST(ReflectTest, MapAndDictionaryReflection) {
    alib6::test::CountingMemoryResource tracker;

    {
        std::map<std::string, int> scores = {
            {"alice", 95},
            {"bob", 88},
            {"charlie", 92}
        };

        auto doc = alib6::to_adata(scores, &tracker);
        EXPECT_TRUE(doc.is_object());
        EXPECT_EQ(doc["alice"].to<int>(), 95);
        EXPECT_EQ(doc["bob"].to<int>(), 88);

        std::map<std::string, int> restored;
        bool ok = alib6::from_adata(restored, doc);
        EXPECT_TRUE(ok);
        EXPECT_EQ(restored.size(), 3);
        EXPECT_EQ(restored["charlie"], 92);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, SchemaGenerationAndObjectMagicKey) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto schema = alib6::generate_schema<ServerCluster>(&tracker);
        EXPECT_TRUE(schema.is_object());

        // 验证 OBJECT MAGIC KEY [ALIB6_OBJ] 承接类级约束
        EXPECT_TRUE(schema.object().contains(alib6::data::magic_key_for_schema_restr));
        EXPECT_NE(schema[alib6::data::magic_key_for_schema_restr].to<std::string_view>().find("MIN_KEYS 1"), std::string_view::npos);

        // 验证成员级约束 (MIN / MAX)
        EXPECT_TRUE(schema.object().contains("base_port"));
        auto port_schema = schema["base_port"][0].to<std::string_view>();
        EXPECT_NE(port_schema.find("MIN"), std::string_view::npos);
        EXPECT_NE(port_schema.find("MAX"), std::string_view::npos);

        // 验证与 Validator 联动
        ServerCluster cluster;
        cluster.cluster_name = "US-East";
        cluster.base_port = 8080;
        cluster.nodes["node-1"] = SubConfig{"10.0.0.1", 8081};

        auto cluster_doc = alib6::to_adata(cluster, &tracker);
        alib6::Validator validator(schema);
        auto val_res = validator.validate(cluster_doc);
        EXPECT_TRUE(val_res.success);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, SchemaOptionalOnNestedStructMember) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto schema = alib6::generate_schema<FogAppConfig>(&tracker);
        EXPECT_TRUE(schema.is_object());

        // window 是对象型 schema 节点, 成员级 OPTIONAL 必须落进 [ALIB6_OBJ] 规则串
        // (旧实现 append_to_rule 只认 node[0], 对对象型节点静默丢弃)
        ASSERT_TRUE(schema.object().contains("window"));
        auto& win_schema = schema["window"];
        EXPECT_TRUE(win_schema.is_object());
        ASSERT_TRUE(win_schema.object().contains(alib6::data::magic_key_for_schema_restr));
        auto win_rule = win_schema[alib6::data::magic_key_for_schema_restr].to<std::string_view>();
        EXPECT_NE(win_rule.find("TYPE OBJECT"), std::string_view::npos);
        EXPECT_NE(win_rule.find("OPTIONAL"), std::string_view::npos);

        // width 显式 required; height 未标记走默认策略 (同样是 "TYPE INT")
        EXPECT_NE(win_schema["width"][0].to<std::string_view>().find("REQUIRED"), std::string_view::npos);
        EXPECT_EQ(win_schema["height"][0].to<std::string_view>(), "TYPE INT");

        // std::optional<Struct> 成员同样受影响
        auto opt_schema = alib6::generate_schema<OptionalWindowHolder>(&tracker);
        ASSERT_TRUE(opt_schema.object().contains("window"));
        auto& opt_win = opt_schema["window"];
        EXPECT_TRUE(opt_win.is_object());
        ASSERT_TRUE(opt_win.object().contains(alib6::data::magic_key_for_schema_restr));
        EXPECT_NE(opt_win[alib6::data::magic_key_for_schema_restr].to<std::string_view>().find("OPTIONAL"), std::string_view::npos);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, ValidatorOptionalNestedStructKeyLevelBoundary) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto schema = alib6::generate_schema<FogAppConfig>(&tracker);
        alib6::Validator validator(schema, &tracker);

        // 1. window 整键缺席 -> optional 只管"键在不在", 整棵子树免检
        //    壳仍会补出 (default_value 需要壳作载体), 但壳内一律不强制 required
        {
            alib6::AData doc(&tracker);
            doc["max_steps"] = 64;
            auto r = validator.validate(doc);
            EXPECT_TRUE(r.success);
            EXPECT_TRUE(r.recorded_errors.empty());
            EXPECT_TRUE(doc.object().contains("window"));
            EXPECT_TRUE(doc["window"].is_object());
            EXPECT_EQ(doc["window"].object().size(), 0);
        }

        // 2. window 完整在场 -> 正常通过
        {
            alib6::AData doc(&tracker);
            doc["max_steps"] = 64;
            doc["window"]["width"] = 1024;
            doc["window"]["height"] = 768;
            auto r = validator.validate(doc);
            EXPECT_TRUE(r.success);
            EXPECT_TRUE(r.recorded_errors.empty());
        }

        // 3. window 在场但残缺 -> 进入 Window 层按本层规矩办事, width 是 required, 照样报错 (optional 不传染)
        {
            alib6::AData doc(&tracker);
            doc["max_steps"] = 64;
            doc["window"]._set_object();
            auto r = validator.validate(doc);
            EXPECT_FALSE(r.success);
            bool hit = false;
            for (const auto& e : r.recorded_errors) {
                if (e.find("Required child 'width'") != std::string::npos) hit = true;
            }
            EXPECT_TRUE(hit);
        }

        // 4. window 在场且只有 height -> 同样报 width 缺失
        {
            alib6::AData doc(&tracker);
            doc["max_steps"] = 64;
            doc["window"]["height"] = 768;
            auto r = validator.validate(doc);
            EXPECT_FALSE(r.success);
            EXPECT_FALSE(r.recorded_errors.empty());
        }

        // 5. std::optional<Struct> 缺席免检 / 残缺报错
        {
            auto opt_schema = alib6::generate_schema<OptionalWindowHolder>(&tracker);
            alib6::Validator opt_validator(opt_schema, &tracker);

            alib6::AData absent(&tracker);
            absent["mode"] = 2;
            EXPECT_TRUE(opt_validator.validate(absent).success);

            alib6::AData partial(&tracker);
            partial["mode"] = 2;
            partial["window"]._set_object();
            EXPECT_FALSE(opt_validator.validate(partial).success);
        }
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, ValidatorOptionalShellMaterializesSubtreeDefaults) {
    alib6::test::CountingMemoryResource tracker;

    {
        // Window { defval(10) x; defval(100) y } + [[optional]] window:
        // 键缺席 -> optional 免检, 且子树里的 default_value 必须照常落地
        auto schema = alib6::generate_schema<DefaultsHolder>(&tracker);
        ASSERT_TRUE(schema.object().contains("window"));
        // default_value 落在对象型节点的数组表达 [规则, 默认值] 上
        ASSERT_TRUE(schema["window"]["width"].is_array());
        EXPECT_EQ(schema["window"]["width"].array().size(), 2u);
        EXPECT_EQ(schema["window"]["width"][1].to<int>(), 10);

        alib6::Validator validator(schema, &tracker);

        // 1. 整键缺席: 不报错, 且默认值通过补出的壳落进 doc
        {
            alib6::AData doc(&tracker);
            doc["mode"] = 2;
            auto r = validator.validate(doc);
            EXPECT_TRUE(r.success);
            EXPECT_TRUE(r.recorded_errors.empty());
            ASSERT_TRUE(doc.object().contains("window"));
            EXPECT_EQ(doc["window"]["width"].to<int>(), 10);
            EXPECT_EQ(doc["window"]["height"].to<int>(), 100);

            // 联动 from_adata: schema 默认值进入 C++ 结构体
            DefaultsHolder holder;
            EXPECT_TRUE(alib6::from_adata(holder, doc));
            EXPECT_EQ(holder.window.width, 10u);
            EXPECT_EQ(holder.window.height, 100u);
        }

        // 2. 部分在场: 在场键照常严格校验, 缺失键用默认值补齐
        {
            alib6::AData doc(&tracker);
            doc["mode"] = 2;
            doc["window"]["width"] = 5;
            auto r = validator.validate(doc);
            EXPECT_TRUE(r.success);
            EXPECT_EQ(doc["window"]["width"].to<int>(), 5);
            EXPECT_EQ(doc["window"]["height"].to<int>(), 100);
        }

        // 3. 空对象在场: 同样补齐
        {
            alib6::AData doc(&tracker);
            doc["mode"] = 2;
            doc["window"]._set_object();
            EXPECT_TRUE(validator.validate(doc).success);
            EXPECT_EQ(doc["window"]["width"].to<int>(), 10);
            EXPECT_EQ(doc["window"]["height"].to<int>(), 100);
        }
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, ValidatorRelaxedMissPropagatesIntoNestedSubtree) {
    alib6::test::CountingMemoryResource tracker;

    {
        // optional sub -> { required gadget; plain } -> gadget { default_value level }
        // sub 缺席: sub 是 optional 壳, gadget 是 required 但无自身默认,
        // 应继续补壳+relaxed 下钻, 让 gadget.level 的默认值落地, 而不是因 gadget 缺失报错
        auto schema = alib6::generate_schema<NestedHolder>(&tracker);
        ASSERT_TRUE(schema.object().contains("sub"));
        EXPECT_NE(schema["sub"][alib6::data::magic_key_for_schema_restr].to<std::string_view>().find("OPTIONAL"), std::string_view::npos);

        alib6::Validator validator(schema, &tracker);

        alib6::AData doc(&tracker);
        doc["mode"] = 1;
        auto r = validator.validate(doc);
        EXPECT_TRUE(r.success);
        EXPECT_TRUE(r.recorded_errors.empty());
        ASSERT_TRUE(doc.object().contains("sub"));
        ASSERT_TRUE(doc["sub"].object().contains("gadget"));
        EXPECT_EQ(doc["sub"]["gadget"]["level"].to<int>(), 42);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, ValidatorOptionalScalarMemberRegression) {
    alib6::test::CountingMemoryResource tracker;

    {
        // 未加注解的标量成员默认 required, optional 标量缺席免检 (回归旧行为)
        struct PlainConfig {
            [[=alib6::attr::schema::optional{}]]
            int age{0};
            std::string username{"admin"};
        };

        auto schema = alib6::generate_schema<PlainConfig>(&tracker);
        EXPECT_EQ(schema["age"][0].to<std::string_view>(), "TYPE INT OPTIONAL");

        alib6::Validator validator(schema, &tracker);

        alib6::AData ok_doc(&tracker);
        ok_doc["username"] = "root";
        auto r1 = validator.validate(ok_doc);
        EXPECT_TRUE(r1.success);
        EXPECT_TRUE(r1.recorded_errors.empty());

        alib6::AData bad_doc(&tracker);
        auto r2 = validator.validate(bad_doc);
        EXPECT_FALSE(r2.success);
        bool hit = false;
        for (const auto& e : r2.recorded_errors) {
            if (e.find("Required child 'username'") != std::string::npos) hit = true;
        }
        EXPECT_TRUE(hit);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

struct UserLoggerConfig {
    [[=alib6::attr::rename<"header">{}]]
    std::string app_name{"LearnVulkan"};

    [[=alib6::attr::alias<"enable_back_pressure">{}]]
    bool back_pressure{true};

    size_t consumer_count{8};
    size_t fetch_message_count_max{256};
};

struct ActualLoggerConfig {
    size_t consumer_count{1};
    size_t fetch_message_count_max{128};
    bool enable_back_pressure{false};
    size_t back_pressure_multiply{4};
    size_t maximum_message_count{100'000};
};

struct ActualFactoryConfig {
    std::string header{""};
    int def_level{2};
};

TEST(ReflectTest, HeterogeneousFillMatchingAndRenameAlias) {
    alib6::test::CountingMemoryResource tracker;

    {
        // 1. Struct-to-Struct 异类静态对齐填充
        UserLoggerConfig user_cfg;
        user_cfg.app_name = "VKEngine";
        user_cfg.back_pressure = true;
        user_cfg.consumer_count = 4;
        user_cfg.fetch_message_count_max = 512;

        ActualLoggerConfig actual_logger;
        ActualFactoryConfig actual_factory;

        alib6::fill_matching(actual_logger, user_cfg);
        alib6::fill_matching(actual_factory, user_cfg);

        // 验证同名字段正确填充
        EXPECT_EQ(actual_logger.consumer_count, 4);
        EXPECT_EQ(actual_logger.fetch_message_count_max, 512);
        // 验证 alias 别名正确映射 (back_pressure -> enable_back_pressure)
        EXPECT_TRUE(actual_logger.enable_back_pressure);
        // 验证未匹配字段保留原结构体默认值
        EXPECT_EQ(actual_logger.back_pressure_multiply, 4);
        EXPECT_EQ(actual_logger.maximum_message_count, 100'000);

        // 验证 rename 重命名正确映射 (app_name -> header)
        EXPECT_EQ(actual_factory.header, "VKEngine");
        EXPECT_EQ(actual_factory.def_level, 2);

        // 2. AData 多目标异类解构提取 (extract_from)
        alib6::AData doc(&tracker);
        doc["header"] = "GameStudio";
        doc["consumer_count"] = 16;
        doc["enable_back_pressure"] = true;

        ActualLoggerConfig log_extracted;
        ActualFactoryConfig factory_extracted;
        bool ok = alib6::extract_from(doc, log_extracted, factory_extracted);
        EXPECT_TRUE(ok);

        EXPECT_EQ(log_extracted.consumer_count, 16);
        EXPECT_TRUE(log_extracted.enable_back_pressure);
        EXPECT_EQ(log_extracted.maximum_message_count, 100'000); // 缺省走默认
        EXPECT_EQ(factory_extracted.header, "GameStudio");
        EXPECT_EQ(factory_extracted.def_level, 2); // 缺省走默认
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

struct TopLevelAppConfig {
    std::string language{"zh_cn"};
    UserLoggerConfig logger{};

    [[=alib6::attr::fill_by<"logger">{}]]
    ActualLoggerConfig actual_logger{};

    [[=alib6::attr::fill_by<"logger">{}]]
    ActualFactoryConfig actual_factory{};
};

TEST(ReflectTest, AutomaticFillByAttributeDuringFromAData) {
    alib6::test::CountingMemoryResource tracker;

    {
        alib6::AData doc(&tracker);
        doc["language"] = "en_us";
        doc["logger"]["header"] = "AutoEngine";
        doc["logger"]["enable_back_pressure"] = true;
        doc["logger"]["consumer_count"] = 32;
        doc["logger"]["fetch_message_count_max"] = 1024;

        TopLevelAppConfig app_cfg;
        alib6::from_adata(app_cfg, doc);

        // 验证常规字段正常反序列化
        EXPECT_EQ(app_cfg.language, "en_us");
        EXPECT_EQ(app_cfg.logger.app_name, "AutoEngine");
        EXPECT_TRUE(app_cfg.logger.back_pressure);
        EXPECT_EQ(app_cfg.logger.consumer_count, 32);

        // 验证 fill_by 字段被自动异类填充
        EXPECT_EQ(app_cfg.actual_logger.consumer_count, 32);
        EXPECT_EQ(app_cfg.actual_logger.fetch_message_count_max, 1024);
        EXPECT_TRUE(app_cfg.actual_logger.enable_back_pressure);
        EXPECT_EQ(app_cfg.actual_logger.maximum_message_count, 100'000); // 默认值

        EXPECT_EQ(app_cfg.actual_factory.header, "AutoEngine");
        EXPECT_EQ(app_cfg.actual_factory.def_level, 2); // 默认值

        // 验证 fill_by 字段在 to_adata 中自动被 skip，不污染导出的 JSON 结构
        auto exported = alib6::to_adata(app_cfg, &tracker);
        EXPECT_TRUE(exported.is_object());
        EXPECT_TRUE(exported.object().contains("language"));
        EXPECT_TRUE(exported.object().contains("logger"));
        EXPECT_FALSE(exported.object().contains("actual_logger"));
        EXPECT_FALSE(exported.object().contains("actual_factory"));

        // 验证 fill_by 字段在 generate_schema 中自动被 skip
        auto schema = alib6::generate_schema<TopLevelAppConfig>(&tracker);
        EXPECT_TRUE(schema.is_object());
        EXPECT_TRUE(schema.object().contains("language"));
        EXPECT_TRUE(schema.object().contains("logger"));
        EXPECT_FALSE(schema.object().contains("actual_logger"));
        EXPECT_FALSE(schema.object().contains("actual_factory"));
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

TEST(ReflectTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        UserProfile p;
        p.username = "very_long_string_to_ensure_dynamic_heap_allocation_and_pmr_check";
        p.endpoint.ip = "192.168.1.254";

        auto doc = alib6::to_adata(p, &tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);

        auto schema = alib6::generate_schema<UserProfile>(&tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);
    }

    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}

struct PriorityTestSource {
    // 基础同名 (Origin: score 1)
    std::string title{"origin_title"};

    // 重命名 (Rename: score 2)
    [[=alib6::attr::rename<"tag">{}]]
    std::string custom_tag{"rename_tag"};

    // 别名指向 title (Alias: score 3)
    [[=alib6::attr::alias<"title">{}]]
    std::string alias_title{"alias_title_winner"};

    // 别名指向 tag (Alias: score 3)
    [[=alib6::attr::alias<"tag">{}]]
    std::string alias_tag{"alias_tag_winner"};
};

struct PriorityTestTarget {
    std::string title{""};
    std::string tag{""};
};

TEST(ReflectTest, FillMatchingPriorityAliasOverRenameOverOrigin) {
    PriorityTestSource src;
    PriorityTestTarget tgt;

    alib6::fill_matching(tgt, src);

    // 验证 alias (Score 3) 胜过 origin (Score 1)
    EXPECT_EQ(tgt.title, "alias_title_winner");

    // 验证 alias (Score 3) 胜过 rename (Score 2)
    EXPECT_EQ(tgt.tag, "alias_tag_winner");
}

struct TypeMismatchSource {
    std::string count{"not_a_number"}; // 同名但是 string
    int width{1920};                   // 兼容类型
};

struct TypeMismatchTarget {
    int count{999};                     // 默认 999，因类型不可赋值安全跳过
    int width{0};                       // 匹配并成功赋值为 1920
};

TEST(ReflectTest, FillMatchingTypeIncompatibilityGracefulSkip) {
    TypeMismatchSource src;
    TypeMismatchTarget tgt;

    alib6::fill_matching(tgt, src);

    // count 因类型不兼容 (= 赋值不支持) 被安全跳过，保留原本默认值 999
    EXPECT_EQ(tgt.count, 999);
    // width 类型兼容，正常赋值 1920
    EXPECT_EQ(tgt.width, 1920);
}
