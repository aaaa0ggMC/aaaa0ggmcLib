# PMR 多目标日志系统 (alib6.log)

`alib6.log` 是一套具备全链路 PMR 内存管理的高性能异步/同步日志库。支持丰富流式操作符、ANSI 终端彩色标签、多目标分发、过滤器管道与开箱即用的预制日志器。

---

## 1. 架构总览

- **`Logger`**：管理多目标（`Console`、`File`、`RotateFile`）分发与消费者线程队列。
- **`LogFactory`**：轻量级日志工厂，自动注入模块标签与日志等级过滤。
- **`StreamedContext`**：支持 `<<` 流式写入，内置丰富 Manipulator 算子。
- **`aout` Prefab**：开箱即用的全局预制日志器。

---

## 2. 快速上手

### 2.1 基础用法
```cpp
import alib6;

using namespace alib6;

int main() {
    // 1. 初始化 Logger (默认启动 1 个后台异步消费工作线程)
    Logger logger;
    logger.append_mod<lot::Console>("console");

    // 2. 创建特定模块的工厂
    LogFactory lg(logger, "NetworkWorker");

    // 3. 流式日志输出
    lg(Severity::Info) << "Worker started on thread " << std::this_thread::get_id() << endlog;
    lg(Severity::Warn) << LOG_COLOR1(Yellow) << "High latency detected!" << endlog;
    lg(Severity::Error) << LOG_COLOR1(Red) << "Connection reset by peer." << endlog;

    return 0;
}
```

### 2.2 同步直写 vs 异步多消费者模式配置
```cpp
// 同步模式：0 消费者线程，当前线程直接分发写入
Logger sync_logger(LoggerConfig{.consumer_count = 0});

// 异步模式：2 个后台工作线程，128 批拉取，启用背压保护
Logger async_logger(LoggerConfig{
    .consumer_count = 2,
    .fetch_message_count_max = 128,
    .enable_back_pressure = true
});
```

---

## 3. 丰富的 Manipulators 操纵符

| 操纵符 | 描述与示例 |
| :--- | :--- |
| `endlog` / `fls` | 结束当前日志行并立即推送到消费队列 |
| `LOG_COLOR1(Color)` | 设置前景色（如 `Red`, `Green`, `Yellow`, `Blue`, `Cyan`） |
| `LOG_COLOR3(Fg, Bg, Mod)` | 同时设置前景色、背景色与样式（如 `Bold`, `Blink`） |
| `log_source()` | 自动捕获当前日志调用处的源文件路径与行号 |
| `log_tfmt(fmt)` | 单次临时格式化算子（如 `lg << log_tfmt("{:.2f}") << 3.14159`） |
| `log_bin(ptr, len)` | 十六进制 / 二进制内存转储视图 |
| `log_omit(data, max_len)` | 超长内容自动截断省略号输出 |
| `log_stacktrace()` | 打印当前调用栈回溯信息 |

---

## 4. 过滤器与管道规则 (`:filters`)

支持为特定目标或全局添加过滤器，实现根据级别、标签或正则表达式丢弃消息：

```cpp
// 过滤掉所有 Verbose/Debug 级别的低优先级消息
logger.append_filter<lof::LevelFilter>("lvl_filter", Severity::Info);
```

---

## 5. 开箱即用的 `aout` Prefab (`:prefab`)

在无需手动配置复杂的 Logger 架构时，直接使用 `aout`：

```cpp
aout << LOG_COLOR1(Green) << "✨ Hello from alib6 preset logger!" << endlog;
aout(Severity::Warn) << "Warning message with value: " << 42 << fls;
```

---

## 6. 非侵入式高性能格式化扩展 (`log.fastfmt` / `:fastfmt`)

`alib6.log` 提供了独立且非侵入的 `:fastfmt` 模块，通过全局重载 `write_to_log(pmr::string& target, const T& val)` 自由扩展任何标准库与第三方类型的零开销序列化：

```cpp
import alib6;
// 或独立按需导入：
import alib6.log.fastfmt;

// 1. 原生指针与空指针（直接 16 进制转字符，避免虚表抽象）
int num = 42;
aout << "Pointer: " << &num << ", Null: " << nullptr << endlog;

// 2. std::chrono 时间与时长
using namespace std::chrono_literals;
aout << "Latency: " << 150ms << ", Frame: " << 16666us << endlog;

// 3. 源码调用位置
aout << "Location: " << std::source_location::current() << endlog;

// 4. 标准库结构化容器
std::optional<int> opt = 100;
std::pair<std::string, int> pair{"port", 8080};
aout << "Config: " << opt << ", Pair: " << pair << endlog;

// 5. GLM 向量与四元数快速格式化
glm::vec3 pos(1.0f, 2.5f, -3.0f);
aout << "Entity Position: " << pos << endlog;
```

---

## 7. 日志系统吞吐与开销实测 (Release 模式)

在 AMD Ryzen 9 8945HX 与 GCC 16 Release 模式下实测同步与异步性能指标（并与 `alib5` 进行同机对照）：

### 6.1 单线程微基准测试 (250,000 条消息采样)
```txt
---------------------------------------------------------
1. 同步直写模式 (consumer_count = 0)
TimeCost         : 33.36 ms
RunTimes         : 250000
Average          : 133.44 ns / 行 (吞吐: ~750 万条/秒，超越 alib5 152.9 ns)
ShortestAvg      : 125.94 ns
CV               : 3.02%
---------------------------------------------------------
2. 异步队列生产模式 (consumer_count = 1)
TimeCost         : 83.53 ms
RunTimes         : 250000
Average          : 334.14 ns / 行 (吞吐: ~299 万条/秒，超越 alib5 425.1 ns)
ShortestAvg      : 323.17 ns
CV               : 2.10%
---------------------------------------------------------
3. 异步彩色标签流式注入 (color)
TimeCost         : 83.55 ms
RunTimes         : 250000
Average          : 334.21 ns / 行
ShortestAvg      : 311.04 ns
CV               : 9.39%
---------------------------------------------------------
4. 异步源码位置捕获 (log_source)
TimeCost         : 137.30 ms
RunTimes         : 250000
Average          : 549.20 ns / 行
ShortestAvg      : 533.93 ns
CV               : 1.80%
---------------------------------------------------------
```

### 6.2 异步多线程高并发吞吐测试 (4 生产者 -> 2 消费者)
```txt
---------------------------------------------------------
Logger Async Multi-Thread Throughput (4 Producers -> 2 Consumers)
Total Logs       : 400,000 条
Total Time       : 244.49 ms
Throughput       : 1,636,061 logs/sec (单条仅需 611.22 ns，超越 alib5 149 万/s)
---------------------------------------------------------
```
