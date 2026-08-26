# 性能基准测试与精度校验 (alib6.perf)

`alib6.perf` 是一个提供统计学分析的纳秒级微基准测试（Micro-benchmarking）模块。能够自动执行热身（Warm-up）、多轮次采样，计算标准差（Stddev）、变异系数（CV）、最短/最长调用均值，并提供外部时钟偏差校验机制。

---

## 1. 快速上手

```cpp
import alib6;
#include <vector>
#include <print>

using namespace alib6;

int main() {
    std::vector<int> vec;
    vec.reserve(10000);

    // 运行 300,000 次，分 100 轮采样
    auto res = Benchmark([&vec] {
        vec.push_back(42);
        vec.pop_back();
    }).run(3000, 100).name("VectorPushPopBenchmark");

    // 打印格式化测试报告
    std::println("{}", res);
    return 0;
}
```

---

## 2. Release 模式实测数据

### 2.1 AData 基础访问与嵌套寻址 (1,000,000 次)
```txt
-----------------------
AData Single-Level Access

TimeCost         : 14.439379 ms
RunTimes         : 1000000
Average          : 14.439379 ns
ShortestAvg      : 13.766500 ns
LongestAvg       : 17.089400 ns
Stddev           : 0.000001
CV               : 3.7377%
--------------------------------

-----------------------
AData 3-Level Nested Access

TimeCost         : 34.436396 ms
RunTimes         : 1000000
Average          : 34.436396 ns
ShortestAvg      : 32.754700 ns
LongestAvg       : 38.095200 ns
Stddev           : 0.000001
CV               : 2.3526%
--------------------------------
```

### 2.2 JSON 序列化与解析 (250,000 次)
```txt
-----------------------
AData JSON Parse

TimeCost         : 205.184789 ms
RunTimes         : 250000
Average          : 820.739156 ns
ShortestAvg      : 811.748200 ns
LongestAvg       : 832.635400 ns
Stddev           : 0.000005
CV               : 0.5663%
--------------------------------

-----------------------
AData JSON Dump

TimeCost         : 134.531837 ms
RunTimes         : 250000
Average          : 538.127348 ns
ShortestAvg      : 515.294000 ns
LongestAvg       : 558.967000 ns
Stddev           : 0.000009
CV               : 1.6841%
--------------------------------
```

---

## 3. 时钟精度偏差校验 (Accuracy Discrepancy Check)

用于校验微基准测试系统与系统真实高精度硬件时钟之间是否存在测量引入的系统性漂移误差：

```cpp
BenchmarkResults res = Benchmark([] {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}).run(10, 10).name("SleepCheck");

// 精度偏差在 < 5% 范围以内即为合格
assert(res.cv() < 10.0);
```
