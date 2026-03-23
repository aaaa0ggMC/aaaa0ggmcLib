# adata

## 目录
- [adata](#adata)
  - [目录](#目录)
  - [总纲](#总纲)
  - [当前Features与规划](#当前features与规划)
    - [Features](#features)
    - [规划](#规划)
  - [性能](#性能)
    - [VS nlohmann/json](#vs-nlohmannjson)
    - [校验性能](#校验性能)
      - [输出](#输出)
    - [加载json性能,主要得益于rapidjson SAX模式](#加载json性能主要得益于rapidjson-sax模式)
      - [输出](#输出-1)
  
## 总纲
AData是一个动态的数据与配置文件解决方案,具备类似动态语言如Python/Javascript等的灵活性与不错的安全性(具体的各种BUG我还没测试出来,需要漫长的维护期)与性能,同时支持数据校验.AData并不绑定一个数据存储形式(JSON,XML,TOML...)而是力图成为一个比较通用的数据解决方案,类似这些数据存储形式的IR.

## 当前Features与规划
### Features
<details>
<summary>支持类似动态语言的食用方式</summary>

> **核心体验：** 像用 Python/JS 一样操作 C++ 强类型数据，内置缓存机制，拒绝冗余拷贝。

```cpp

```

</details>

<details>
<summary>预制菜已经支持 JSON</summary>

> **核心体验：** 高度集成的 IO 流。不仅是解析，更是对数据流的极致控制。

```cpp
    // 1. 灵活装载 (Memory / File / alib5 FileEntry)
    doc.load_from_memory(R"({"status": "ok"})");
    doc.dump_to_file("./config.json");

    // 2. 深度集成：与 alogger 完美配合，高效序列化
    aout << doc << fls; 

    // 3. 极致定制：通过 JSONConfig 实现精细控制
    data::JSONConfig cfg {
        .float_precision = 2,
        .sort_object = true, // 字典序输出
        .filter = [](auto key, auto& node) { 
            return (key == "secret") ? Discard : Keep; // 动态过滤字段
        }
    };
    std::cout << doc.str(data::JSON(cfg)); // 缩写版 dump

```

</details>

<details>
<summary>支持数据校验与归一. 小学习成本, 但好用!</summary>

> **核心体验：** 声明式 Schema。将繁琐的 `if(contains)` 判定转化为简洁的 DSL 约束。

```cpp
    // 1. 定义 Schema：支持类型约束、必填项、默认值填充
    Validator vl = Validator::from_adata(R"({
        "name" : "REQUIRED TYPE STRING VALIDATE not_empty",
        "id"   : ["TYPE INT", 1024, "<- 自动填充默认值"],
        "arr"  : ["TYPE ARRAY MIN 1", ["TYPE INT"]] 
    })");

    // 2. 一键校验：获取详细的错误链路
    AData input; 
    input["name"].set_null(); 

    auto result = vl.validate(input);
    if (!result.success) {
        // 输出示例: ".name : Expected String, got Null"
        std::cout << "Error: " << result.recorded_errors[0];
    }

```
</details>

### 规划
- 支持Acessor这层访问封装从而彻底地理论上支持任何数据类型而非目前的JSON
- (等C++26)支持静态反射绑定到类
- 加入一些有用的validates预制菜
- 提供对TOML的解析(因为老版本的ADATA就是提供了对JSON&TOML的解析的)

## 性能


### VS nlohmann/json
| 性能指标 | Release **AData** | Release **nlohmann** | 性能差异 (AData 优势) |
| --- | --- | --- | --- |
| **平均耗时 (Average)** | **13.99 ns** | 17.52 ns | **+20.1%** |
| **最短耗时 (Shortest)** | **11.87 ns** | 14.25 ns | +16.7% |
| **最长耗时 (Longest)** | **41.56 ns** | 79.55 ns | **+47.8%** |
| **总计用时 (TimeCost)** | 13.99 ms | 17.52 ms | -3.52 ms |
| **标准差 (Stddev)** | 1.59e-06 | 3.94e-06 | 波动更小 |
| **变异系数 (CV)** | **11.41%** | 22.50% | **稳定性高 1 倍** |
| **运行次数 (RunTimes)** | 1,000,000 | 1,000,000 | - |
代码:
```cpp
    AData d;
    d["main"] = "Hello";
    d["sub"][0] = 1;

    nlohmann::json j;
    j["main"] = "Hello";
    j["sub"][0] = 1;

    aout << Benchmark([&d]{
        // 这里返回的是引用哦
        d["sub"][0].value().transform<int>() ++;
    }).run(1000,1000).name("Release AData") << fls;

    aout << Benchmark([&j]{
        j["sub"][0].get_ref<nlohmann::json::number_integer_t&>()++;
    }).run(1000,1000).name("Release nlohmann") << fls;
```

### 校验性能
```cpp    
    AData schema;
    schema.load_from_memory(R"({
        "name" : ["TYPE STRING","Hello"],
        "obj" : {
            "name" : ["","What?"],
            "ids" : [
                "TYPE ARRAY MIN 1 MAX 100",
                ["TYPE INT",0]    
            ]
        }
    })");
    Validator vl;
    {
        // 编译大致用时
        Clock clk;
        vl.from_adata(schema);
        double t = clk.get_offset();
        std::cout << "Compilation costed " << t*1000 << " us" << std::endl;
    }

    AData nerd;
    vl.validate(nerd);
    aout << nerd << fls;

    aout << Benchmark([&vl]{
        AData d;
        vl.validate(d);
    }).run(1000, 100).name("validate cost") << fls;
```

#### 输出
```txt
Compilation costed 40.368 us
{
  "name" : "Hello",
  "obj" : {
    "name" : "What?",
    "ids" : [
      0
    ]
  }
}

-----------------------
validate cost

TimeCost        :75.94456699999998ms
RunTimes        :100000
Average         :759.445654693991ns
ShortestAvgCall :718.317ns
LongestAvgCall  :851.296ns
Stddev          :1.8462227e-05
CV              :2.431%
--------------------------------
```

### 加载json性能,主要得益于rapidjson SAX模式
```cpp
    aout << Benchmark([]{
        AData schema;
        schema.load_from_memory(R"({
            "name" : ["TYPE STRING","Hello"],
            "obj" : {
                "name" : ["","What?"],
                "ids" : [
                    "TYPE ARRAY MIN 1 MAX 100",
                    ["TYPE INT",0]    
                ]
            }
        })");
    }).run(1000, 100).name("parse cost") << fls;
```
#### 输出
```txt
-----------------------
parse cost

TimeCost        :70.16121799999998ms
RunTimes        :100000
Average         :701.6121526248753ns
ShortestAvgCall :680.3240000000001ns
LongestAvgCall  :736.127ns
Stddev          :9.9766135e-06
CV              :1.422%
--------------------------------
</pre>
```
