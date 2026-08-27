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

    struct [[=alib6::attr::schema::extra<"MIN_KEYS 1">{}]] ServerCluster {
        std::string cluster_name{"APAC-1"};
        [[=alib6::attr::schema::range{1.0, 65535.0}]]
        int base_port{8000};
        std::map<std::string, SubConfig> nodes;
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

