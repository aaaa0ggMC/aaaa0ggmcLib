# aalgorithm 各算法的时间复杂度的简要测试

- [aalgorithm 各算法的时间复杂度的简要测试](#aalgorithm-各算法的时间复杂度的简要测试)
  - [前言](#前言)
    - [测试平台](#测试平台)
    - [Log](#log)
  - [冒泡排序](#冒泡排序)
    - [基础信息](#基础信息)
    - [Code](#code)
    - [执行结果](#执行结果)
  - [选择排序](#选择排序)
    - [基础信息](#基础信息-1)
    - [Code](#code-1)
    - [执行结果](#执行结果-1)
  - [插入排序](#插入排序)
    - [基础信息](#基础信息-2)
    - [Code](#code-2)
    - [执行结果](#执行结果-2)
  - [希尔排序(Div2处理)](#希尔排序div2处理)
    - [基础信息](#基础信息-3)
    - [Code](#code-3)
    - [执行结果](#执行结果-3)
  - [希尔排序(Knuth序列)](#希尔排序knuth序列)
    - [基础信息](#基础信息-4)
    - [Code](#code-4)
    - [执行结果](#执行结果-4)
  - [希尔排序(Hibbard序列)](#希尔排序hibbard序列)
    - [基础信息](#基础信息-5)
    - [Code](#code-5)
    - [执行结果](#执行结果-5)
  - [快排+Lomuto分区策略](#快排lomuto分区策略)
    - [基础信息](#基础信息-6)
    - [Code](#code-6)
    - [执行结果](#执行结果-6)
  - [快排+Hoare分区策略](#快排hoare分区策略)
    - [基础信息](#基础信息-7)
    - [Code](#code-7)
    - [执行结果](#执行结果-7)
  - [快排+MedianOfThree分区(Hoare改进)策略](#快排medianofthree分区hoare改进策略)
    - [基础信息](#基础信息-8)
    - [Code](#code-8)
    - [执行结果](#执行结果-8)
  - [std::sort](#stdsort)
    - [基础信息](#基础信息-9)
    - [Code](#code-9)
    - [执行结果](#执行结果-9)
  - [测试程序](#测试程序)

## 前言
- 这个文件并非单纯性能测试,虽然你能大致上看出性能差距
- 使用的数据为随机的,因此显然没比较最好最坏情况,这里便是尝试拟合一般情况下的时间复杂度
- 前面几行为对模型进行R^2排序,后面两行第一行为在当前数据下N^a拟合的a的值,后面的EstimatedModel为根据a进行简要拟合
- 对于大部分算法不具备证明意义,比如Hoare显示是O(N)但是R^2排序下O(N logN)还是比O(N)优先
- 数据量不是很大,最大也就2^24个int,但是我觉得在查看时间复杂度情况上已经有点说服力了

### 测试平台
- AMD Ryzen 9 8945HX (32) @ 5.46 GHz
- gcc (GCC) 15.2.1 20260209
- CMake Release模式(aalgorithm由于是单纯模板库因此不需要考虑aaaa0ggmcLib的问题)

### Log
- 代码没clear,数据似乎要重新测 2026/03/25
- 重测完毕 2026/03/26

## 冒泡排序
### 基础信息
- 经过优化的版本,在一次冒泡没有数据后会提前剪枝走掉
- 期待时间复杂的O(N^2)
### Code
```cpp
using namespace algo::sort;
bubble(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━┓
┋ Name          ┊ Bubble ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 16     ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5      ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━┛
...
Progress: 16/16 (100%) 189528.82ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9999998573175357  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.983564321836772   ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9403583434648086  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9205213446396148  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.28106617322680233 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.9780768623069618  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N^2)              ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┛
```

## 选择排序
### 基础信息
- 挺标准的,没啥优化
- 期待时间复杂度O(N^2)
### Code
```cpp
using namespace algo::sort;
selection(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━┓
┋ Name          ┊ Selection ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 16        ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5         ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5         ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━┛
...
Progress: 16/16 (100%) 29138.63ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9999984938972022 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.9833315609554095 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9405821447119348 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9207538313280251 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.2810853069531061 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.9856668282256602 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N^2)             ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┛
```

## 插入排序
### 基础信息
- 挺标准的插入排序
- 期待O(N^2)
### Code
```cpp
insertion(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━┓
┋ Name          ┊ Insertion ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 16        ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5         ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5         ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━┛
...
Progress: 16/16 (100%) 7663.70ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9998570844073114 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.9833318639226502 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9423137225857623 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9229938819523974 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.2866019466218021 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.8515918243857759 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N^2)             ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┛
```

## 希尔排序(Div2处理)
### 基础信息
- 没啥
- 期待小于2的 N^a
### Code
```cpp
using namespace algo::sort;
shell<ShellGapType::Div2>(wait_sort.begin(), wait_sort.end(),std::less<int>());    
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━┓
┋ Name          ┊ Shell(/2) ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 20        ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5         ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5         ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━┛
...
Progress: 16/20 (80%) 326.18ms
Progress: 20/20 (100%) 18623.24ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                     ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9877802449844908      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9785225296764763      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9681039130333857      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.9443136296822127      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.2811888465518388      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.4468539162149205      ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N^1.4468539162149205) ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━┛
```

## 希尔排序(Knuth序列)
### 基础信息
- 没啥
- 期待小于2的 N^a
### Code
```cpp
using namespace algo::sort;
shell<ShellGapType::Knuth>(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┓
┋ Name          ┊ Shell(Knuth) ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 20           ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5            ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5            ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━┛
...
Progress: 16/20 (80%) 195.23ms
Progress: 20/20 (100%) 4135.86ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9999512104630159  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9984259150033461  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9387037524436933  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.8668046267983167  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.35762249107668687 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.1132945334842599  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N logN)           ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┛
```

## 希尔排序(Hibbard序列)
### 基础信息
- 没啥
- 期待小于2的 N^a
### Code
```cpp
using namespace algo::sort;
shell<ShellGapType::Hibbard>(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┓
┋ Name          ┊ Shell(Hibbard) ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 20             ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5              ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5              ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┛
...
Progress: 16/20 (80%) 235.23ms
Progress: 20/20 (100%) 5867.59ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9948990582035149  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9895060006192573  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9661760165878565  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.9085567826098971  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.32861357788805345 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.173623463024128   ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N logN)           ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┛
```

## 快排+Lomuto分区策略
### 基础信息
- 没啥
- 期待O(N logN)
### Code
```cpp
using namespace algo::sort;
quick<PartitionLomuto>(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓
┋ Name          ┊ Quick(Lomuto) ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 24            ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5             ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5             ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━┛
...
Progress: 16/24 (66%) 117.57ms
Progress: 20/24 (83%) 2302.71ms
Progress: 24/24 (100%) 44381.59ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9999941508372716  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9991897802737817  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9336640407126297  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.8603320551155554  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.31287318041150197 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.0749892628157327  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N logN)           ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┛
```

## 快排+Hoare分区策略
### 基础信息
- 没啥
- 期待O(N logN)
### Code
```cpp
using namespace algo::sort;
quick<PartitionHoare>(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┓
┋ Name          ┊ Quick(Hoare) ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 24           ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5            ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5            ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━┛
...
Progress: 16/24 (66%) 183.99ms
Progress: 20/24 (83%) 2586.23ms
Progress: 24/24 (100%) 47905.97ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9999948111797309 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9993005999361424 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9325172104282812 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.8586251003956078 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.3143196605122689 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.01058918826169   ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N)               ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┛
```

## 快排+MedianOfThree分区(Hoare改进)策略
### 基础信息
- 没啥
- 期待O(N logN)
### Code
```cpp
using namespace algo::sort;
quick<PartitionMedianThree>(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┓
┋ Name          ┊ Quick(MedianThree) ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 24                 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5                  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5                  ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┛
...
Progress: 16/24 (66%) 183.55ms
Progress: 20/24 (83%) 2734.82ms
Progress: 24/24 (100%) 49491.47ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9999633064897918 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9995076737031604 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9304153881362093 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.8558600547961538 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.3166169964905837 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.0120299377159614 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N)               ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┛
```

## std::sort
### 基础信息
- 没啥
- 预期O(N logN)
### Code
```cpp
std::sort(wait_sort.begin(), wait_sort.end(),std::less<int>());
```
### 执行结果
```txt
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━┓
┋ Name          ┊ std::sort ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Rows          ┊ 24        ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ DifferentData ┊ 5         ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┫
┋ Repeat        ┊ 5         ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━┛
...
Progress: 16/24 (66%) 107.65ms
Progress: 20/24 (83%) 2023.45ms
Progress: 24/24 (100%) 38847.15ms
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
┋ Model         ┊ R^2                 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N log N)    ┊ 0.9999924354108739  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N)          ┊ 0.9992388226848768  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^2)        ┊ 0.9332145642822396  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(N^3)        ┊ 0.8596976256088293  ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ O(log N)      ┊ 0.31345079563579664 ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ N^a           ┊ 1.067949420256624   ┋
┣┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╋┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┫
┋ EstimateModel ┊ O(N logN)           ┋
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┛
```


## 测试程序
```cpp
#include <alib5/alogger.h>
#include <alib5/aalgorithm.h>
#include <alib5/compact/make_table.h>
#include <random>
#include <vector>
#include <cmath>
#include <string>
#include <queue>

using namespace alib5;
using namespace alib5::algo;

double calculate_r2(const std::vector<double>& x, const std::vector<double>& y){
    if(x.size() != y.size() || x.empty()) return 0.0;
    size_t n = x.size();
    
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0, sum_yy = 0;
    for(size_t i = 0; i < n; ++i){
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
        sum_yy += y[i] * y[i];
    }
    double numerator = (n * sum_xy - sum_x * sum_y);
    double denominator = (n * sum_xx - sum_x * sum_x) * (n * sum_yy - sum_y * sum_y);
    
    if(denominator <= 0.0) return 0.0; 
    return (numerator * numerator) / denominator; 
}

double calculate_slope(const std::vector<double>& x, const std::vector<double>& y) {
    if(x.size() != y.size() || x.size() < 2) return 0.0;
    size_t n = x.size();
    
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for(size_t i = 0; i < n; ++i){
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
    }
    
    double denominator = (n * sum_xx - sum_x * sum_x);
    if(denominator == 0.0) return 0.0;
    
    return (n * sum_xy - sum_x * sum_y) / denominator;
}

struct ModelResult{
    std::string model_name;
    double r_squared;

    bool operator<(const ModelResult& other) const {
        return r_squared < other.r_squared; 
    }
};

constexpr size_t rows = 16;
constexpr size_t times = 5;
constexpr size_t sort_times = 5;
int main() {
    Clock global;
    std::vector<int> wait_sort;
    
    constexpr static std::string_view name = "Insertion";
    auto func = [&] {
        using namespace algo::sort;
        insertion(wait_sort.begin(), wait_sort.end(),std::less<int>());
    };

    log_table info([](auto & op){
        op[99][0] << "Name";
        op[99][1] << name;
        
        op[100][0] << "Rows";
        op[100][1] << rows;
        
        op[101][0] << "DifferentData";
        op[101][1] << times;
        
        op[102][0] << "Repeat";
        op[102][1] << sort_times;
    });
    info.config = info.modern_dot();
    aout << info << endlog;

    std::mt19937_64 generator;
    std::uniform_int_distribution<> dist; 
    std::vector<int> basic_data;
    std::vector<double> time_costs;
    std::vector<double> n_values; 
    size_t data_size = 1;
    for(size_t i = 0; i < rows; ++i){
        data_size *= 2;
        n_values.push_back(static_cast<double>(data_size));

        if(basic_data.capacity() < data_size) basic_data.reserve(data_size);
        double sum = 0;
        for(size_t t = 0; t < times; ++t){
            for(size_t m = 0; m < data_size; ++m){
                basic_data.emplace_back(dist(generator));
            }
            for(size_t tt = 0; tt < sort_times; ++tt){
                wait_sort = basic_data;
                Clock clk;
                func();
                sum += clk.get_all();
            }
            basic_data.clear();
        }
        sum /= sort_times * times;
        time_costs.emplace_back(sum);
        {
            auto tag = LOG_COLOR1(Gray);
            float percent = (float)(i+1) / rows;
            if(percent >= 0.8f){
                tag = LOG_COLOR1(Green);
            }else if(percent > 0.6f){
                tag = LOG_COLOR1(Cyan);
            }else if(percent > 0.4f){
                tag = LOG_COLOR1(Blue);
            }else{
                tag = LOG_COLOR1(Gray);
            }
            aout << "Progress: " 
                << tag << i+1 << "/" << rows 
                << " (" << tag << (int)(percent * 100) << "%" << ") " << LOG_COLOR1(None) 
                << log_tfmt("{:.2f}") << global.get_all() << "ms"
                << endlog;
        }
    }

    std::priority_queue<ModelResult> results_pq;
    size_t data_points = n_values.size();
    std::vector<double> x_logn(data_points), x_n(data_points), x_nlogn(data_points), x_n2(data_points), x_n3(data_points);
    for(size_t i = 0; i < data_points; ++i) {
        double n = n_values[i];
        x_logn[i]  = std::log2(n);
        x_n[i]     = n;
        x_nlogn[i] = n * std::log2(n);
        x_n2[i]    = n * n;
        x_n3[i]    = n * n * n;
    }
    results_pq.push({"O(log N)",   calculate_r2(x_logn, time_costs)});
    results_pq.push({"O(N)",       calculate_r2(x_n, time_costs)});
    results_pq.push({"O(N log N)", calculate_r2(x_nlogn, time_costs)});
    results_pq.push({"O(N^2)",     calculate_r2(x_n2, time_costs)});
    results_pq.push({"O(N^3)",     calculate_r2(x_n3, time_costs)});

    std::vector<double> log_n_filtered, log_t_filtered;
    size_t start_idx = rows / 2; 
    for(size_t i = start_idx; i < data_points; ++i) {
        if(time_costs[i] > 0.0) { 
            log_n_filtered.push_back(std::log(n_values[i]));
            log_t_filtered.push_back(std::log(time_costs[i]));
        }
    }
    double estimated_a = calculate_slope(log_n_filtered, log_t_filtered);

    log_table r2s([results_pq,estimated_a](auto & op) mutable {
        op[0][0] << LOG_COLOR1(Blue) << "Model"; 
        op[0][1] << LOG_COLOR1(Blue) << "R^2"; 

        size_t i = 1;
        while(!results_pq.empty()){
            const auto& best = results_pq.top();
            auto tag = LOG_COLOR1(None);

            if(best.r_squared > 0.95)tag = LOG_COLOR1(Green);
            else if(best.r_squared <= 0.7)tag = LOG_COLOR1(Gray);

            op[i][0] << best.model_name;
            op[i][1] << tag << best.r_squared;
            ++i;
            results_pq.pop();
        }

        ++i;
        op[i][0] << "N^a";
        op[i][1] << estimated_a;
        ++i;
        op[i][0] << "EstimateModel";
        if(estimated_a >= 0.9 && estimated_a <= 1.05){
            op[i][1] << "O(N)";
        }else if (estimated_a > 1.05 && estimated_a <= 1.25){
            op[i][1] << "O(N logN)";
        }else if (estimated_a > 1.8 && estimated_a <= 2.2){
            op[i][1] << "O(N^2)";
        }else{
            op[i][1] << "O(N^" << estimated_a << ")";
        }
    });
    r2s.config = r2s.modern_dot();
    r2s.add_restore_tag(LOG_COLOR1(None));
    aout << r2s << endlog;
    return 0;
}
```