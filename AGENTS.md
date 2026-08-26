# `alib6` 开发者与 AI 智能体编码规范与架构指南 (AGENTS.md)

本文档面向所有参与 `alib6` 代码库开发、重构、扩展与维护的开发者及 AI 智能体（Agents）。在进行任何代码修改或新增功能时，**必须严格遵守以下规范**。

---

## 1. 模块化与目录架构规范

### 1.1 模块与分区命名
- **顶层主模块**：`export module alib6;`（位于 `include/alib6/main.cppm`），通过 `export import alib6.core;` 导出核心功能。
- **核心聚合模块**：`export module alib6.core;`（位于 `include/alib6/core/core.cppm`），统一聚合导出所有子分区。
- **核心接口子分区**：`export module alib6.core:<partition>;`（位于 `include/alib6/core/<partition>.cppm`）。
  - 例如：`:concepts`, `:types`, `:memory`, `:bstd`, `:debug`, `:error`, `:defer`, `:str`, `:time`, `:sys`, `:io`, `:ref`, `:parser`, `:router`, `:cmd`, `:storage`。

### 1.2 接口（`.cppm`）与实现（`.cpp`）分工
1. **接口定义（`include/alib6/core/*.cppm`）**：
   - 包含模块导出声明、模板实现、Concepts、内联轻量函数。
   - **禁止**在 `.cppm` 中引入庞大或污染性的平台头文件（如 `<windows.h>`, `<psapi.h>`, `<unistd.h>` 等）。
2. **重型实现（`modules/alib6/core/*.cpp`）**：
   - 模块声明使用 `module alib6.core;`。
   - 存放重型非模板函数算法、复杂 I/O 遍历以及底层操作系统 API 封装。
   - 优势：增量编译时修改实现**绝不触发**下游 `import` 文件的级联重编译。

---

## 2. 内存管理与 PMR 规范（最高优先级）

`alib6` 是一套全体系支持多态内存资源（PMR）的高性能库。

1. **默认与可注入内存资源**：
   - 所有涉及动态内存分配的结构、容器与函数，必须提供 `memory_resource* mem = get_default_resource()` 参数。
   - 示例：
     ```cpp
     pmr::string unescape(std::string_view in, memory_resource* mem = get_default_resource());
     TraverseData traverse_files(std::string_view path, TraverseConfig cfg = {}, memory_resource* mem = get_default_resource(), ErrorWrapper err = {});
     ```
2. **严禁使用 `static thread_local` 缓冲返回 `string_view`**：
   - 历史上 `alib5` 使用 `thread_local` 静态缓冲返回 `std::string_view`，会导致在同一表达式内多次调用或嵌套调用时产生数据覆盖和悬垂 UB。
   - `alib6` 统一返回 `pmr::string` 并支持指定内存池。
3. **PMR 容器与子分配器传递**：
   - 当嵌套容器或向子函数传递分配器时，使用 `container.get_allocator().resource()` 获取底层 `memory_resource*`。

---

## 3. Concepts 概念设计规范

1. **少而精，通用优先**：
   - 统一使用 `Cast<From, To>`（即 `std::convertible_to<From, To>`）代替宽泛的 `IsStringLike` / `IsPathLike`。
   - 统一在 [`include/alib6/core/concepts.cppm`](file:///home/aaaa0ggmc/Projs/aaaa0ggmcLib/include/alib6/core/concepts.cppm) 中集中维护全局 Concepts。
2. **标准库概念命名习惯**：
   - Concepts 命名避免 `Is` 前缀，采用领域名词或动词（如 `IndexableContainer`, `CanExtendString`）。

---

## 4. 引用子系统规范（`alib6.core:ref`）

1. **操作符必须 100% 透明穿透（Transparent Operator Pass-Through）**：
   - `RefWrapper` 和 `MultiRefWrapper` 的 `operator[]`、`operator()`、`operator*`、`operator->`、隐式类型转换以及所有算术/比较运算符，必须**完全穿透至底层被引用的元素**。
2. **内存与索引偏移必须采用显式方法（Explicit Navigation）**：
   - 严禁用 `operator[]` 或 `operator()` 来做容器内部的索引步进或层级跳转。
   - 必须使用显式 API：
     - `r.next_element()` / `r.prev_element()` / `r.advance(offset)`
     - `mr.parent()` / `mr.child(idx)` / `mr.with_index(idx)`

---

## 5. 错误处理规范（`alib6.core:error`）

1. **`ErrorWrapper` 零开销注入**：
   - 潜在失败函数统一在末尾添加 `ErrorWrapper err = {}`。
   - 内部调用 `err.report(...)`，在无需报警时利用编译器常量折叠达到 0 额外开销。
2. **自动捕获调用点 `std::source_location`**：
   - 借助 `PanicFormat` 和 `CodeWithLocation` 实现编译期自动捕获行号，严禁滥用宏进行源位置注入。

---

## 6. 测试规范与 PMR 泄漏防范（强制要求）

测试体系分为两大目标：
- **`test6`**（位于 `modules/alib6_test/`）：轻量级快速联调测试。
- **`gtest6`**（位于 `tests/alib6/`）：完备的 Google Test 单元测试套件。

### ⚠️ 必须包含 PMR 内存外泄与隔离测试 Section
在为任何包含内存分配的模块编写/维护单测时，**必须**在测试文件中保留并添加 `PMRMemoryIsolationAndLeakCheck` 测试用例：
1. 使用 [`tests/alib6/pmr_tracker.h`](file:///home/aaaa0ggmc/Projs/aaaa0ggmcLib/tests/alib6/pmr_tracker.h) 中提供的 `alib6::test::CountingMemoryResource` 追踪器。
2. 验证函数分配确实走了传入的 `memory_resource*`，而非偷偷走全局堆。
3. 验证当对象析构离开作用域后，`tracker.has_leak()` 必须为 `false` 且 `tracker.live_bytes() == 0`。

**示例代码模版**：
```cpp
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import alib6;

TEST(MyModuleTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        auto obj = alib6::my_function("input...", &tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0); // 确保内存走入 tracker
    }

    // 作用域析构后，内存必须 100% 归还，绝无泄漏
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
```

---

## 7. 编译与测试指令速查

```bash
# 1. 构建核心库与集成联调测试
xmake b aaaa0ggmcLib6 && xmake b test6 && xmake r test6

# 2. 构建并运行全量 Google Test 单元测试套件
xmake b gtest6 && xmake r gtest6
```
