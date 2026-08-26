# 多态数据引擎与反射系统 (alib6.data)

`alib6.data` 提供了一套无缝的数据处理体系，包含类似 Python 字典的 `AData` 多态树、零拷贝 JSON/TOML 双向序列化、DSL 校验器、C++26 静态反射以及 i18n 国际化翻译组件。

---

## 1. AData 核心多态字典树 (`:kernel`)

`AData` 拥有 4 种基础大类型：`Null`、`Value`（包含 `Int64`, `Double`, `Bool`, `String`）、`Array` 与 `Object`。
支持多级路径动态扩展与自赋值安全移动：

```cpp
import alib6;
#include <print>

using namespace alib6;

int main() {
    AData doc;
    // 1. 基础标量无缝赋值
    doc["app_name"] = "alib6_cluster";
    doc["port"] = 8080;
    doc["active"] = true;

    // 2. 数组自动扩容 (Auto-expand)
    doc["nodes"][0] = "192.168.1.10";
    doc["nodes"][1] = "192.168.1.11";

    // 3. 递归对象路径创建
    doc["metrics"]["limits"]["cpu"] = 4.0;

    // 4. JSON Pointer 极速快速跳转
    auto* val = doc.jump_ptr("/metrics/limits/cpu");
    if (val) {
        std::println("CPU Limit: {}", val->value().get_double());
    }
}
```

---

## 2. JSON 与 TOML 序列化 (`:json` & `:toml`)

支持精确控制浮点精度、格式化缩进、字典序排序输出及自定义字段过滤器：

```cpp
// 导出为格式化 JSON
std::string json_out = data::dump_json(doc, {.indent = 2, .float_precision = 2});

// 导出为 TOML
std::string toml_out = data::dump_toml(doc);
```

---

## 3. Validator DSL 架构与自动填充 (`:validator`)

支持声明式 Schema 校验与默认值注入：

```cpp
AData schema;
schema.load_from_memory(R"({
    "app_name": "REQUIRED TYPE STRING",
    "port":     ["TYPE INT", 8080, "<- 缺省时自动注入默认值"],
    "nodes":    ["TYPE ARRAY MIN 1", ["TYPE STRING"]]
})");

Validator val;
val.from_adata(schema);

AData config;
config["app_name"] = "AuthService";
// 此时未提供 port，校验时将自动注入 8080！

auto res = val.validate(config);
if (res.success) {
    std::println("配置校验通过，生效端口: {}", config["port"].value().get_int64()); // 8080
}
```

---

## 4. C++26 静态反射与异类字段匹配 (`:reflect`)

`alib6.data:reflect` 基于标准静态反射特性，实现了结构体零样板代码的双向序列化与异类结构体字段迁移。

### 4.1 基础反射序列化
```cpp
struct DatabaseConfig {
    std::string host{"localhost"};
    int port{5432};
    std::vector<std::string> pools;
};

// C++ 结构体 -> AData
DatabaseConfig db{.host = "10.0.0.1", .port = 3306, .pools = {"read", "write"}};
AData doc = reflect::to_adata(db);

// AData -> C++ 结构体
DatabaseConfig restored = reflect::from_adata<DatabaseConfig>(doc);
```

### 4.2 异类字段填充 (`fill_matching` 与 `[[fill_by]]`)
当需要从第三方大型结构体（如 `Logger`）中提取特定字段填充到配置结构体（如 `LoggerConfig`）时，可通过 `fill_matching` 自动按字段名与类型匹配复制，或通过字段别名声明：

```cpp
struct SourceState {
    int log_level{3};
    std::string log_file{"app.log"};
    double internal_scratch{0.0};
};

struct TargetConfig {
    int log_level{0};
    std::string log_file{""};
};

SourceState src{.log_level = 2, .log_file = "service.log", .internal_scratch = 999.9};
TargetConfig tgt;

// 自动匹配同名字段注入
reflect::fill_matching(src, tgt);
// tgt.log_level == 2, tgt.log_file == "service.log"
```

---

## 5. 多语言国际化翻译组件 (`:translator`)

`alib6.data:translator` 支持多语言动态切换、插值格式化、点号平铺路径（`FlattenTranslator`）与只读高效快照：

```cpp
Translator tr;
tr.set_locale_data("zh_CN", R"({
    "welcome": "欢迎光临，{}",
    "server": {
        "start": "服务器正在端口 {} 启动"
    }
})");

tr.set_locale_data("en_US", R"({
    "welcome": "Welcome, {}",
    "server": {
        "start": "Server is starting on port {}"
    }
})");

tr.switch_locale("zh_CN");
std::println("{}", tr.translate("welcome", "Alice"));
// 输出: 欢迎光临，Alice

std::println("{}", tr.translate("server.start", 8080));
// 输出: 服务器正在端口 8080 启动
```

---

## 6. AData 实际性能基准测试 (Release 实测)

在 AMD Ryzen 9 8945HX 与 GCC 16 Release 模式下测速：

### 6.1 键值访问速度 (1,000,000 次)
```txt
-----------------------
AData Single-Level Access
TimeCost         : 14.43 ms
RunTimes         : 1000000
Average          : 14.43 ns
ShortestAvg      : 13.76 ns
CV               : 3.73%
--------------------------------
-----------------------
AData 3-Level Nested Access
TimeCost         : 34.43 ms
RunTimes         : 1000000
Average          : 34.43 ns
ShortestAvg      : 32.75 ns
CV               : 2.35%
--------------------------------
```

### 6.2 JSON 解析与转储性能 (250,000 次)
```txt
-----------------------
AData JSON Parse
TimeCost         : 205.18 ms
RunTimes         : 250000
Average          : 820.73 ns
CV               : 0.56%
--------------------------------
-----------------------
AData JSON Dump
TimeCost         : 134.53 ms
RunTimes         : 250000
Average          : 538.12 ns
CV               : 1.68%
--------------------------------
```
