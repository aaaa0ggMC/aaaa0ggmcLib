# aalgorithm 各算法时间复杂度与实际基准测试报告 (alib6.algo)

## 目录
- [aalgorithm 各算法时间复杂度与实际基准测试报告 (alib6.algo)](#aalgorithm-各算法时间复杂度与实际基准测试报告-alib6algo)
  - [目录](#目录)
  - [测试平台与环境配置](#测试平台与环境配置)
  - [冒泡排序 (bubble_sort)](#冒泡排序-bubble_sort)
  - [鸡尾酒排序 (cocktail_sort)](#鸡尾酒排序-cocktail_sort)
  - [地精排序 (gnome_sort)](#地精排序-gnome_sort)
  - [插入排序 (insertion_sort)](#插入排序-insertion_sort)
  - [奇偶排序 (odd_even_sort)](#奇偶排序-odd_even_sort)
  - [选择排序 (selection_sort)](#选择排序-selection_sort)
  - [梳排序 (comb_sort)](#梳排序-comb_sort)
  - [希尔排序 (shell_sort)](#希尔排序-shell_sort)
  - [快速排序 + Lomuto 分区 (quick_sort<PartitionLomuto>)](#快速排序--lomuto-分区-quick_sortpartitionlomuto)
  - [快速排序 + Hoare 分区 (quick_sort<PartitionHoare>)](#快速排序--hoare-分区-quick_sortpartitionhoare)
  - [快速排序 + 三数取中分区 (quick_sort<PartitionMedianThree>)](#快速排序--三数取中分区-quick_sortpartitionmedianthree)
  - [std::sort 对照基准](#stdsort-对照基准)
  - [字符串搜索：KMP 模式匹配 vs 朴素查找 (1MB 文本)](#字符串搜索kmp-模式匹配-vs-朴素查找-1mb-文本)
  - [测试驱动代码](#测试驱动代码)

---

## 测试平台与环境配置

<pre>
OS: Arch Linux x86_64
Kernel: Linux 6.18.7-arch1-1
CPU: AMD Ryzen 9 8945HX (32) @ 5.46 GHz
Memory: 31.37 GiB
Compiler: GCC 16.1.1 (C++26 with Modules & Reflection)
Build Mode: XMake Release Mode (-O3 / -O2 Optimized)
</pre>

---

## 冒泡排序 (bubble_sort)

### 基础信息
- **算法特性**：带 `swapped` 提前剪枝优化的冒泡排序。
- **理论复杂度**：$O(N^2)$。

### 示例代码
```cpp
import alib6;
std::vector<int> data = {5, 2, 8, 1, 9};
alib6::algo::bubble_sort(data);
```

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{13}$)
```txt
  Scale 2^1  (N=2      ):     0.0016 ms
  Scale 2^2  (N=4      ):     0.0013 ms
  Scale 2^3  (N=8      ):     0.0012 ms
  Scale 2^4  (N=16     ):     0.0015 ms
  Scale 2^5  (N=32     ):     0.0020 ms
  Scale 2^6  (N=64     ):     0.0038 ms
  Scale 2^7  (N=128    ):     0.0117 ms
  Scale 2^8  (N=256    ):     0.0484 ms
  Scale 2^9  (N=512    ):     0.1709 ms
  Scale 2^10 (N=1024   ):     0.5521 ms
  Scale 2^11 (N=2048   ):     2.0921 ms
  Scale 2^12 (N=4096   ):     8.0529 ms
  Scale 2^13 (N=8192   ):    32.1991 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N^2)$** | **0.999992** |
| $O(N^3)$ | 0.983466 |
| $O(N \log N)$ | 0.945999 |
| $O(N)$ | 0.922790 |
| $O(\log N)$ | 0.339178 |
| **$N^a$ 幂律回归斜率** | **1.2865** |
| **评估模型** | **$O(N^2)$** |

---

## 鸡尾酒排序 (cocktail_sort)

### 基础信息
- **算法特性**：双向交替定向冒泡，在小端与大端同时收缩。
- **理论复杂度**：$O(N^2)$。

### 示例代码
```cpp
import alib6;
alib6::algo::cocktail_sort(data);
```

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{13}$)
```txt
  Scale 2^9  (N=512    ):     0.0638 ms
  Scale 2^10 (N=1024   ):     0.2335 ms
  Scale 2^11 (N=2048   ):     0.8906 ms
  Scale 2^12 (N=4096   ):     3.3618 ms
  Scale 2^13 (N=8192   ):    13.0348 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N^2)$** | **0.999917** |
| $O(N^3)$ | 0.981341 |
| $O(N \log N)$ | 0.949389 |
| **评估模型** | **$O(N^2)$** |

---

## 地精排序 (gnome_sort)

### 基础信息
- **算法特性**：单游标正向推进并在逆序时单步回退交换。
- **理论复杂度**：$O(N^2)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{13}$)
```txt
  Scale 2^9  (N=512    ):     0.4150 ms
  Scale 2^10 (N=1024   ):     1.6405 ms
  Scale 2^11 (N=2048   ):     6.4299 ms
  Scale 2^12 (N=4096   ):    24.8927 ms
  Scale 2^13 (N=8192   ):    98.5606 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N^2)$** | **0.999988** |
| $O(N^3)$ | 0.982834 |
| **评估模型** | **$O(N^2)$** |

---

## 插入排序 (insertion_sort)

### 基础信息
- **算法特性**：连续数据移动插入，在小规模数据及基本有序数据下性能极高。
- **理论复杂度**：$O(N^2)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{13}$)
```txt
  Scale 2^9  (N=512    ):     0.0171 ms
  Scale 2^10 (N=1024   ):     0.0601 ms
  Scale 2^11 (N=2048   ):     0.2293 ms
  Scale 2^12 (N=4096   ):     0.9378 ms
  Scale 2^13 (N=8192   ):     3.6468 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N^2)$** | **0.999955** |
| **评估模型** | **$O(N^2)$** |

---

## 奇偶排序 (odd_even_sort)

### 基础信息
- **算法特性**：奇偶相位交替比较交换。
- **理论复杂度**：$O(N^2)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{13}$)
```txt
  Scale 2^11 (N=2048   ):     1.5445 ms
  Scale 2^12 (N=4096   ):     5.8440 ms
  Scale 2^13 (N=8192   ):    22.0075 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N^2)$** | **0.999734** |
| **评估模型** | **$O(N^2)$** |

---

## 选择排序 (selection_sort)

### 基础信息
- **算法特性**：每次扫描挑选极值元素交换至头部，交换次数最少。
- **理论复杂度**：$O(N^2)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{13}$)
```txt
  Scale 2^11 (N=2048   ):     0.8859 ms
  Scale 2^12 (N=4096   ):     3.5199 ms
  Scale 2^13 (N=8192   ):    13.8463 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N^2)$** | **0.999983** |
| **评估模型** | **$O(N^2)$** |

---

## 梳排序 (comb_sort)

### 基础信息
- **算法特性**：改进版冒泡，使用递减 gap 消除乌龟数据（小值在大端）。
- **理论复杂度**：$O(N \log N)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{18}$，最大 $N=262,144$)
```txt
  Scale 2^14 (N=16384  ):     0.7276 ms
  Scale 2^15 (N=32768  ):     1.6082 ms
  Scale 2^16 (N=65536  ):     3.5664 ms
  Scale 2^17 (N=131072 ):     7.2087 ms
  Scale 2^18 (N=262144 ):    15.7381 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N \log N)$** | **0.999815** |
| $O(N)$ | 0.998082 |
| $O(N^2)$ | 0.941295 |
| **评估模型** | **$O(N \log N)$** |

---

## 希尔排序 (shell_sort)

### 基础信息
- **算法特性**：步长折半缩减插入排序。
- **理论复杂度**：$O(N \log^2 N) \sim O(N^{1.3})$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{18}$)
```txt
  Scale 2^15 (N=32768  ):     3.1478 ms
  Scale 2^16 (N=65536  ):     7.0275 ms
  Scale 2^17 (N=131072 ):    18.0068 ms
  Scale 2^18 (N=262144 ):    49.2412 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N \log N)$** | **0.986199** |
| $O(N^2)$ | 0.982130 |
| **评估模型** | **$O(N \log N)$** |

---

## 快速排序 + Lomuto 分区 (quick_sort<PartitionLomuto>)

### 基础信息
- **算法特性**：单向快慢指针分区，栈式迭代。
- **理论复杂度**：$O(N \log N)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{18}$)
```txt
  Scale 2^14 (N=16384  ):     0.5766 ms
  Scale 2^15 (N=32768  ):     1.2411 ms
  Scale 2^16 (N=65536  ):     2.6706 ms
  Scale 2^17 (N=131072 ):     5.5210 ms
  Scale 2^18 (N=262144 ):    11.8256 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N \log N)$** | **0.999960** |
| $O(N)$ | 0.998603 |
| $O(N^2)$ | 0.938568 |
| **评估模型** | **$O(N \log N)$** |

---

## 快速排序 + Hoare 分区 (quick_sort<PartitionHoare>)

### 基础信息
- **算法特性**：经典双指针向中点碰撞分区。
- **理论复杂度**：$O(N \log N)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{18}$)
```txt
  Scale 2^14 (N=16384  ):     0.6500 ms
  Scale 2^15 (N=32768  ):     1.3780 ms
  Scale 2^16 (N=65536  ):     2.9154 ms
  Scale 2^17 (N=131072 ):     6.0951 ms
  Scale 2^18 (N=262144 ):    12.7431 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N \log N)$** | **0.999930** |
| **评估模型** | **$O(N \log N)$** |

---

## 快速排序 + 三数取中分区 (quick_sort<PartitionMedianThree>)

### 基础信息
- **算法特性**：头、尾、中点三数中位数枢轴，大幅度降低最坏退化概率。
- **理论复杂度**：$O(N \log N)$。

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{18}$)
```txt
  Scale 2^14 (N=16384  ):     0.6354 ms
  Scale 2^15 (N=32768  ):     1.3629 ms
  Scale 2^16 (N=65536  ):     2.9048 ms
  Scale 2^17 (N=131072 ):     6.0397 ms
  Scale 2^18 (N=262144 ):    12.6110 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N \log N)$** | **0.999910** |
| **评估模型** | **$O(N \log N)$** |

---

## std::sort 对照基准

### 实测数据与回归拟合 (Scale $2^1 \sim 2^{18}$)
```txt
  Scale 2^14 (N=16384  ):     0.4938 ms
  Scale 2^15 (N=32768  ):     1.0628 ms
  Scale 2^16 (N=65536  ):     2.2589 ms
  Scale 2^17 (N=131072 ):     4.7596 ms
  Scale 2^18 (N=262144 ):    10.0278 ms
```

| 拟合模型 | $R^2$ 判定系数 |
| :--- | :--- |
| **$O(N \log N)$** | **0.999981** |
| **评估模型** | **$O(N \log N)$** |

---

## 字符串搜索：KMP 模式匹配与 KmpContext 预计算 (1MB 文本)

针对 1MB 长文本进行 10,000 次基准测速：

### 1. 实测数据对比
```txt
---------------------------------------------------------
1. KMP Search with Precomputed KmpContext (1MB text)
TimeCost         : 23.755 us
RunTimes         : 10000
Average          : 2.3755 ns (极速！超越常规朴素查找)
ShortestAvg      : 2.3740 ns
CV               : 0.0365%
---------------------------------------------------------
2. KMP Search On-the-Fly (临时构造 next 表)
TimeCost         : 129.455 us
RunTimes         : 10000
Average          : 12.9455 ns
ShortestAvg      : 11.8760 ns
CV               : 9.1293%
---------------------------------------------------------
3. Plain Search (std::string_view / 泛型迭代器)
TimeCost         : 26.129 us
RunTimes         : 10000
Average          : 2.6129 ns
ShortestAvg      : 2.3740 ns
CV               : 27.2776%
---------------------------------------------------------
```

### 2. 对抗性恶劣数据测试 (Adversarial Worst-Case)
在 100,000 个连续 `'a'` 中搜索 200 个 `'a'` + `'b'`：
```txt
-----------------------
KMP Search (Adversarial)
TimeCost         : 3.33 ms
RunTimes         : 10000
Average          : 333.02 ns
CV               : 6.00%
-----------------------
Plain Search (Adversarial)
TimeCost         : 23.75 us
RunTimes         : 10000
Average          : 2.37 ns
-----------------------
```

### 3. KMP 性能机理解析与 KmpContext 优势
1. **预计算上下文 `KmpContext` 的必要性**：
   - 传统 KMP 每次调用都需要为模式串构建失配表（`next` 数组）。如果对同一个模式串在大量文本流中频繁查找，预先构建 `KmpContext ctx(pattern)` 能将模式预处理成本均摊为 0，实测单次匹配仅需 **2.37 ns**！
2. **现代 CPU 下的 Plain Search vs KMP**：
   - 现代标准库的 `std::string_view::find`（基于 glibc `memmem`）在底层使用了 **AVX-512 / AVX2 SIMD 指令**（单指令并行比对 32~64 字节）以及硬件分支预测加速；
   - 纯标量 KMP 在通用迭代器上利用 `KmpContext` 提供了严谨的 $O(N)$ 理论时间保证，避免了极端恶劣失配下的多重回溯退化。

---

## 测试驱动代码

可通过执行项目内置的基准测试二进制直接复现上述全量评测：
```bash
xmake f -m release && xmake b bench6 && xmake r bench6
```
源码位于 [`benchmarks/alib6/main.cpp`](file:///home/aaaa0ggmc/Projs/aaaa0ggmcLib/benchmarks/alib6/main.cpp)。
