# aaaa0ggmc's library gen6 (alib6) 开发者指南与架构文档

## 目录
- [aaaa0ggmc's library gen6 (alib6) 开发者指南与架构文档](#aaaa0ggmcs-library-gen6-alib6-开发者指南与架构文档)
  - [目录](#目录)
  - [前言：从 alib5 到 alib6 的全面蜕变](#前言从-alib5-到-alib6-的全面蜕变)
  - [测试平台](#测试平台)
  - [10 秒快速上手体验](#10-秒快速上手体验)
  - [子系统速查与架构导航](#子系统速查与架构导航)
  - [全链路 PMR 内存模型](#全链路-pmr-内存模型)
  - [基准测试与性能表现 (APerf)](#基准测试与性能表现-aperf)
  - [全量 Google Test 单元测试套件](#全量-google-test-单元测试套件)

---

## 前言：从 alib5 到 alib6 的全面蜕变

`alib6` 是 `alib5` 的全新迭代与全体系现代化重构版本。在保留原有高表达力、爽快 API（如类似 Python dict 的 AData、流式彩色日志、零开销错误注入）的基础上，进行了彻底的架构进化：

1. **全面拥抱 C++20/C++23 模块（Modules）**：
   - 彻底淘汰庞大且污染头文件宏定义的传统 `.h` 组织。
   - 采用 `export module alib6;` 以及细粒度子分区（Partitions），实现极速增量编译，避免下游文件的级联重编译。
2. **全链路 PMR 多态内存资源注入（零全局堆逃逸）**：
   - 所有动态分配容器、字符串、格式化缓冲区全面支持 `std::pmr::memory_resource*` 显式注入。
   - 彻底消除了 `alib5` 历史遗留中 `thread_local` 静态缓冲返回 `string_view` 带来的数据覆盖与悬垂 UB 隐患。
3. **C++26 静态反射（P2996 `<meta>`）赋能**：
   - 引入静态反射元编程，实现结构体字段的零开销双向序列化、`[[fill_by]]` 异类字段自动匹配填充与 Schema 自动推导。
4. **更加纯粹通用的 Concepts 设计**：
   - 废除模糊冗余的 Concepts 概念，全面规范化为标准概念体系。
5. **完备严格的 PMR 泄漏追踪单测体系**：
   - 包含 23 个测试套件、105 个 Google Test 单元测试，内置 `CountingMemoryResource` 内存泄漏检测器，确保 100% 内存归还。

---

## 测试平台

<pre>
OS: Arch Linux x86_64
Kernel: Linux 6.18.7-arch1-1
Shell: bash 5.3.9
CPU: AMD Ryzen 9 8945HX (32) @ 5.46 GHz
Memory: 31.37 GiB
Compiler: GCC 16.1.1 (C++23 / C++26 Reflection Ready)
Build System: XMake v2.9+
</pre>

---

## 10 秒快速上手体验

在 `alib6` 中，只需一条 `import alib6;`，即可畅享从日志、配置、反射、协程到排版表格的全套现代化基础设施：

```cpp
import alib6;
#include <print>

using namespace alib6;

struct ServerConfig {
    std::string host{"127.0.0.1"};
    int port{8080};
    bool debug{true};
};

int main() {
    // 1. 现代化多目标彩色日志
    Logger logger;
    LogFactory lg(logger, "ServerApp");
    logger.append_mod<lot::Console>("console");

    // 2. 优雅的 Defer 清理机制 (RAII Guard)
    $defer { lg << LOG_COLOR1(Green) << "✨ alib6 server closed cleanly." << endlog; };

    // 3. 极速 AData 数据操作与 C++26 静态反射自动映射
    AData doc;
    doc.load_from_memory(R"({"host": "0.0.0.0", "port": 9000, "debug": false})");
    ServerConfig cfg = reflect::from_adata<ServerConfig>(doc);

    lg(Severity::Info) << "Server starting on " 
                       << LOG_COLOR1(Cyan) << cfg.host << ":" << cfg.port << endlog;

    // 4. 现代化美观表格输出
    Table tbl(TableConfig::unicode_rounded());
    tbl[0][0] = "配置项"; tbl[0][1] = "生效值";
    tbl[1][0] = "Host";   tbl[1][1] = cfg.host;
    tbl[2][0] = "Port";   tbl[2][1] = std::to_string(cfg.port);
    tbl[3][0] = "Debug";  tbl[3][1] = cfg.debug ? "true" : "false";

    std::println("{}", tbl);
    return 0;
}
```

---

## 子系统速查与架构导航

`alib6` 体系按功能划分为以下独立且协同的子系统：

| 子系统文档 | 对应模块 | 核心能力与关键功能 |
| :--- | :--- | :--- |
| **[核心工具库 (Core)](./core.md)** | `alib6.core` | PMR 内存基础设施、`ErrorWrapper` 零开销错误处理、`ref/refs` 安全穿透引用、管道解析器 `Parser`、动态路由器 `Router`、CLI 命令树 `Command`、`MonoBitSet`/`FreeList` 存储池 |
| **[多态数据与反射 (Data)](./data.md)** | `alib6.data` | `AData` 多态字典树、JSON/TOML 双向序列化、Validator DSL 校验器、C++26 静态反射（`reflect`）、`[[fill_by]]` 异类注入、多语言国际化 `Translator` |
| **[日志系统 (Logger)](./log.md)** | `alib6.log` | PMR 多目标异步日志队列、彩色紧凑标签渲染、丰富的流式 Manipulators、日志管道与开箱即用的 `aout` Prefab |
| **[实体组件系统 (ECS)](./ecs.md)** | `alib6.ecs` | 现代稀疏集（Sparse Set）架构、零锁高效 View 多组件联合查询、组件依赖绑定与生命周期自动回收 |
| **[现代化表格排版 (Table)](./table.md)** | `alib6.table` | Unicode/ASCII 全风格边框、自动计算 CJK 与 Emoji 终端显示宽度、多行单元格自动对齐与无缝多层表格嵌套 |
| **[性能测试与精度校验 (Perf)](./perf.md)** | `alib6.perf` | 纳秒级统计基准测试 `Benchmark`、正态分布与离散系数（CV）分析、外部时钟精度偏差校验（Discrepancy Check） |
| **[协程与时钟调度 (Co & Clock)](./co_clock.md)** | `alib6.co`, `alib6.clock` | 基于 `std::generator` 的 `Task<T>` 协程任务、`Signal`/`WaitGroup`/`Race` 原子同步原语、`Clock`/`Trigger`/`RateLimiter` 自适应 FPS 限制器 |
| **[经典算法库 (Algo)](./algo.md)** | `alib6.algo` | 细粒度模块化 9 种排序算法（快排含 Lomuto/Hoare/MedianThree）+ 2 种搜索算法（朴素 / KMP）、`InjectPosIterator` 交换过程追踪钩子 |
| **[批量窃取请求队列 (Request)](./request.md)** | `alib6.request` | 多生产者/多消费者双队列批量窃取（Batch Stealing）、PMR 队列优化与 `std::visit` 零虚表请求分发 |

---

## 全链路 PMR 内存模型

在 `alib6` 中，所有涉及堆内存分配的函数与容器均默认接受可注入的 `pmr::memory_resource*` 参数：

```cpp
alib6::test::CountingMemoryResource tracker;

// 将内存资源注入 AData 与字符串解析
AData doc(&tracker);
doc.load_from_memory(R"({"status": "running", "workers": [1, 2, 3]})", &tracker);

// 提取数据同样走传入的追踪内存池
pmr::string status = doc["status"].value().get_str(&tracker);

// 验证内存走入 tracker
assert(tracker.allocated_bytes() > 0);
```

当作用域结束，`doc` 析构后，`tracker.has_leak()` 必定为 `false`，彻底消灭潜在的内存外泄问题。

---

## 基准测试与性能表现 (APerf 实测)

在 AMD Ryzen 9 8945HX 平台与 GCC 16 Release 模式下实测核心子系统性能表现：

### 1. `ext::to_string` vs `std::to_string` (1,000,000 次)
```txt
-----------------------
alib6::ext::to_string (pmr::string)
TimeCost         : 102.25 ms
RunTimes         : 1000000
Average          : 102.25 ns
ShortestAvg      : 93.75 ns
CV               : 7.64%
--------------------------------
-----------------------
std::to_string
TimeCost         : 55.48 ms
RunTimes         : 1000000
Average          : 55.48 ns
ShortestAvg      : 52.21 ns
CV               : 2.37%
--------------------------------
```

### 2. `AData` 多层路径访问性能 (1,000,000 次)
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

### 3. `AData` JSON 解析与序列化 (250,000 次)
```txt
-----------------------
AData JSON Parse
Average          : 820.73 ns
CV               : 0.56%
--------------------------------
-----------------------
AData JSON Dump
Average          : 538.12 ns
CV               : 1.68%
--------------------------------
```

### 4. ECS 10,000 实体 2 组件 View 联合遍历 (50,000 轮)
```txt
-----------------------
ECS 10k Entities 2-Component View Iteration
TimeCost         : 5.57 s
RunTimes         : 50000
Average          : 111.50 us (单实体仅需 11.15 ns)
ShortestAvg      : 100.40 us
CV               : 6.04%
--------------------------------
```

### 5. `Logger` 日志系统同步与异步高并发吞吐 (同机超越 alib5)
```txt
---------------------------------------------------------
1. 同步直写模式 (consumer_count = 0)
   单条平均: 133.44 ns / 行 (吞吐: ~750 万条/秒，超越 alib5 152.9 ns)
---------------------------------------------------------
2. 异步队列生产模式 (consumer_count = 1)
   单条平均: 334.14 ns / 行 (吞吐: ~299 万条/秒，超越 alib5 425.1 ns)
---------------------------------------------------------
3. 异步 4 线程并发生产 -> 2 消费者处理 (400,000 条)
   总耗时: 244.49 ms
   吞吐量: 1,636,061 logs/sec (单条仅需 611.22 ns，超越 alib5 149 万/s)
---------------------------------------------------------
```

---

## 全量 Google Test 单元测试套件

`alib6` 拥有覆盖全模块的自动化 Google Test 套件（`gtest6`）：
```bash
# 编译并运行全量单测
xmake b gtest6 && xmake r gtest6
```

**运行结果**：
```txt
[==========] 105 tests from 23 test suites ran. (803 ms total)
[  PASSED  ] 105 tests.
```
