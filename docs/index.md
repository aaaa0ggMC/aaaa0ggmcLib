# aaaa0ggmc's library gen5 文档

## 目录
- [aaaa0ggmc's library gen5 文档](#aaaa0ggmcs-library-gen5-文档)
  - [目录](#目录)
  - [前言](#前言)
  - [工具库 autil](#工具库-autil)
    - [Perf0 测速ext与std to\_string,release下](#perf0-测速ext与std-to_stringrelease下)
  - [协程库(实验性)与时钟库 aco \& aclock](#协程库实验性与时钟库-aco--aclock)
    - [利用碎片时间](#利用碎片时间)
    - [异步上的支持(实验性质)](#异步上的支持实验性质)
  - [日志库 alogger](#日志库-alogger)

## 前言
简略地介绍aaaa0ggmcLib主要组件的情况以及简单的使用案例

## 工具库 autil
包含alib内置的错误处理系统,简单的字符串数据转换,文件io以及一些增强语言功能的特性.
```cpp
#include <alib5/autil.h>
#include <iostream>

using namespace alib5;

int main(){
    /// 错误处理
    // 不允许捕获,只能有一个
    set_fast_error_callback([](const RuntimeError & err){
        std::cout << std::format("[SV{}]:{} in {}",(int)err.severity,err.data,err.location.function_name())
                  << std::endl;
    });
    // 允许捕获,可以有很多个
    add_error_callback([&](const RuntimeError & err){});
    // 产生错误信息
    invoke_error(0,"This is an error!",Severity::Error);
    invoke_error(0,"Supports std::format {}.",1);

    /// 数据清理
    // 类似go/zig的defer
    {
        struct { std::function<void(std::string_view)> execute; } world = {
            [&](std::string_view target){ std::cout << target << std::endl; }
        };
        auto me = "me";
        $defer { world.execute(me); };
    }
    // 栈式清理,个人觉得在简单的vulkan app初始化中使用还行
    {
        misc::DeferManager defer_mgr;
        defer_mgr.defer([&]{ std::cout << "The last one." << std::endl; });
        char * a = new char[16];
        if(!a)return -1;
        defer_mgr.defer([&]{
            delete [] a;
            std::cout << "deleted a.\n" << std::endl;
        });
        char * b = new char[16];
        if(!b)return -1; // 这里直接return也可以清理掉a,个人觉得很舒服
        defer_mgr.defer([&]{
            delete [] b;
            std::cout << "deleted b.\n" << std::endl;
        });
    }
    
    /// 字符串转换,std::to_string慢一点点,但是能处理更多种类数据,见(perf0)
    // to_string 支持std::format能处理的类型
    std::string_view sv = ext::to_string<false>(10); // false表示不copy,返回string_view
    std::pmr::string str = ext::to_string(true); // 默认为true,std::返回pmr::string使用全局内存池
    // to_T 支持基础数据类型(int,double,...),to_T
    std::from_chars_result result;
    double b = ext::to_T<double>("1.234aab"); // 不提供回写,会invoke_error
    int b2 = ext::to_T<double>("18ddfh",&result); // 提供回写,不会报错

    /// 文件操作
    // 比较简单的api,读取&写入
    std::string target;
    size_t bytes_read = io::read_all("./test.csv", target);
    io::write_all("./test2.csv", target);
    // 遍历文件 + io::FileEntry
    io::TraverseConfig cfg;
    cfg.depth = 1;
    cfg.ignore.emplace_back([](std::string_view fp){return fp.contains(".json");});
    cfg.keep.emplace_back(io::FileEntry::regular);
    io::TraverseData data = io::traverse_files("CMakeFiles",cfg);
    // 这里是因为data简化了获取流程,这个实际上是 for(auto & entry : data.targets_absolute)
    for(io::FileEntry & entry : data){
        // 懒加载,自动尝试获取子文件,不是文件夹会invoke_error
        for(auto& [file_name,sub_entry] : entry){
            // 可以持续遍历
            std::cout << file_name << " ";
        }
    }
    // 可以load_entry
    io::FileEntry entry = io::load_entry(".cmake"); // 不存在会invoke_error
    io::FileEntry entry_may_not_valid = io::load_entry(".cmake_bad",false);

    /// 字符串操作
    std::string a = "a ,b c ,b d";
    // 切割
    std::vector<std::string_view> slices = str::split(a,',');
    std::vector<std::string_view> slices_2 = str::split(a,",b");
    // 大小写转换(实验性质),使用的是ranges,因此如果需要访问原始数据需要转存
    auto upper = str::to_upper("abcde") | std::views::drop(0);
    auto lower = str::to_lower("ABCDE");
    // 其余的包含删除首尾空白字符,转义与反转义(支持绝大多数字符串)

    // 获取CPU品牌
    std::string_view brand = sys::get_cpu_brand();
    // 获取程序内存占用
    sys::ProgramMemUsage us0 = sys::get_prog_mem_usage();
    // 获取全局内存占用
    sys::GlobalMemUsage us1 = sys::get_global_mem_usage();
}
```
参考输出
```txt
[SV4]:This is an error! in main
[SV4]:Supports std::format 1. in main
me
deleted b.
deleted a.
The last one.

[SV4]:"/path/to/project/build/CMakeFiles/CMakeConfigureLog.yaml" is not a directory! in alib5::io::traverse_files
[SV4]:20|filesystem error: directory iterator cannot open directory: Not a directory [/path/to/project/build/CMakeFiles/CMakeConfigureLog.yaml] in alib5::io::traverse_files
[SV4]:"/path/to/project/build/CMakeFiles/cmake.check_cache" is not a directory! in alib5::io::traverse_files
[SV4]:20|filesystem error: directory iterator cannot open directory: Not a directory [/path/to/project/build/CMakeFiles/cmake.check_cache] in alib5::io::traverse_files
[SV4]:"/path/to/project/build/CMakeFiles/TargetDirectories.txt" is not a directory! in alib5::io::traverse_files
[SV4]:20|filesystem error: directory iterator cannot open directory: Not a directory [/path/to/project/build/CMakeFiles/TargetDirectories.txt] in alib5::io::traverse_files
[SV4]:"/path/to/project/build/CMakeFiles/rules.ninja" is not a directory! in alib5::io::traverse_files
[SV4]:20|filesystem error: directory iterator cannot open directory: Not a directory [/path/to/project/build/CMakeFiles/rules.ninja] in alib5::io::traverse_files
```
### Perf0 测速ext与std to_string,release下
```cpp
#include <alib5/aperf.h>

using namespace alib5;

int main(){
    double val = 1234567890;
    std::cout << Benchmark([&val]{
        auto sv = ext::to_string<false>(val);
    }).run(10000,100).name("ext view") << std::endl;
    std::cout << Benchmark([&val]{
        auto cpy = ext::to_string(val);
    }).run(10000,100).name("ext copy") << std::endl;
    std::cout << Benchmark([&val]{
        auto cpy = std::to_string(val);
    }).run(10000,100).name("std copy") << std::endl;
}
```
```txt
-----------------------
ext view

TimeCost        :30.7784ms
RunTimes        :1000000
Average         :30.7784ns
ShortestAvgCall :29.445ns
LongestAvgCall  :41.3181ns
Stddev          :1.61359e-06
CV              :5.243%
--------------------------------
-----------------------
ext copy

TimeCost        :35.4065ms
RunTimes        :1000000
Average         :35.4065ns
ShortestAvgCall :34.5505ns
LongestAvgCall  :40.1308ns
Stddev          :8.64742e-07
CV              :2.442%
--------------------------------
-----------------------
std copy

TimeCost        :30.8461ms
RunTimes        :1000000
Average         :30.8461ns
ShortestAvgCall :29.2076ns
LongestAvgCall  :39.0622ns
Stddev          :1.32394e-06
CV              :4.292%
--------------------------------
```

## 协程库(实验性)与时钟库 aco & aclock
### 利用碎片时间
```cpp
#include <alib5/aco.h>
#include <alib5/aclock.h>

using namespace alib5;

int main(){
    size_t spin = 0;
    // 创建任务
    co::Task task([&]->std::generator<int>{
        while(true){
            // 其实这个就是this_thread::sleep_for
            Timer(std::chrono::milliseconds(1)).wait();
            ++spin;
            co_yield 0;
        }
    });

    // 固定帧率的东西,同时对启动时机要求不是很高
    RateLimiter rl(60); // 60hz
    Clock clk; // 计时器
    while(true){
        if(spin > 1000){
            std::cout << clk.get_all() << "ms:Spinned for 1k" << std::endl;
            spin = 0;
        }
        // rl.wait(); 单纯的sleep
        rl.wait(task); // 可以在等待的时候不断轮训task
    }    
}
```
```txt
1067.69ms:Spinned for 1k
2133.46ms:Spinned for 1k
3200.75ms:Spinned for 1k
4266.81ms:Spinned for 1k
5333.95ms:Spinned for 1k
6400.14ms:Spinned for 1k
7466.95ms:Spinned for 1k
8533.76ms:Spinned for 1k
9600.08ms:Spinned for 1k
10667.3ms:Spinned for 1k
11733.5ms:Spinned for 1k
12800ms:Spinned for 1k
13866.8ms:Spinned for 1k
```
### 异步上的支持(实验性质)
```cpp
#include <alib5/aco.h>
#include <alib5/aclock.h>

using namespace alib5;

int main(){
    co::WaitGroup wg;
    co::Signal sig;
    co::Task may_be_do_something(nap(1)); // 其实就是自旋1ms
    std::jthread th0([guard = wg.make_guard()]{
        Timer(1000).wait(); // 睡眠1s
    });
    std::jthread th1([guard = wg.make_guard()]{
        Timer(2000).wait(); // 睡眠2s
    });
    std::jthread th2([&sig]{
        Timer(500).wait();
        sig.fire();
    });
    
    size_t index = co::any(may_be_do_something, wg,sig);

    std::cout << "Index " << index << " triggered!" << std::endl; 
}
```
```txt
Index 1 triggered!
```

## 日志库 [alogger](./alogger.md)
```cpp

```
