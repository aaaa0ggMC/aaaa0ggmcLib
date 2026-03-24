# aaaa0ggmc's library gen5 文档

## 目录
- [aaaa0ggmc's library gen5 文档](#aaaa0ggmcs-library-gen5-文档)
  - [目录](#目录)
  - [前言](#前言)
  - [测试平台](#测试平台)
  - [工具库 autil](#工具库-autil)
    - [Perf0 测速ext与std to\_string,release下](#perf0-测速ext与std-to_stringrelease下)
  - [协程库(实验性)与时钟库 aco \& aclock](#协程库实验性与时钟库-aco--aclock)
    - [利用碎片时间](#利用碎片时间)
    - [异步上的支持(实验性质)](#异步上的支持实验性质)
  - [日志库 alogger](#日志库-alogger)
  - [数据处理库 adata](#数据处理库-adata)
    - [关于数据类型](#关于数据类型)
    - [示例代码](#示例代码)
    - [perf0 json读取速度测试](#perf0-json读取速度测试)
    - [perf1 单层引用测试](#perf1-单层引用测试)
  - [ECS系统 aecs](#ecs系统-aecs)
  - [比较安全的引用系统 aref](#比较安全的引用系统-aref)
  - [Rustic🦀的崩溃系统 adebug](#rustic的崩溃系统-adebug)
  - [性能测试 aperf](#性能测试-aperf)
  - [算法库(实验性) aalgorithm](#算法库实验性-aalgorithm)
  - [简单的i18n支持 atranlator](#简单的i18n支持-atranlator)
    - [基础Translator和Flatten版本的对比](#基础translator和flatten版本的对比)

## 前言
简略地介绍aaaa0ggmcLib主要组件的情况以及简单的使用案例
```cpp
#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/autil.h>
#include <alib5/adebug.h>

using namespace alib5;

int main() {
    // 1. 开箱即用的精美日志
    Logger logger;
    LogFactory lg(logger, "alib5_intro");
    logger.append_mod<lot::Console>("console");

    // 2. 类似 Go/Zig 的优雅清理机制
    $defer { lg << LOG_COLOR1(Green) << "✨ alib5 shutdown gracefully." << endlog; };

    // 3. 极其爽快的无缝数据操作 (类似 Python dict)
    AData doc;
    doc.load_from_memory(R"({"name": "aaaa0ggmcLib", "version": 5.0, "magic": true})");
    doc["features"][0] = "Modern C++20/23";
    doc["features"][1] = "Aesthetic Logger";

    // 4. Rustic 风格的硬核防御
    panic_if(!doc["magic"].value().transform<bool>(), "Where is the magic?!");

    // 5. 极简的流式输出
    lg(Severity::Info) << "Welcome to " << LOG_COLOR1(Cyan) << doc["name"] 
                       << LOG_COLOR1(None) << " v" << doc["version"] << endlog;

    return 0;
}
```

## 测试平台
<pre>
OS: Arch Linux x86_64
Kernel: Linux 6.18.7-arch1-1
Shell: bash 5.3.9
DE: KDE Plasma 6.5.5
WM: KWin (Wayland)
CPU: AMD Ryzen 9 8945HX (32) @ 5.46 GHz
GPU 1: NVIDIA GeForce RTX 5060 Max-Q / Mobile [Discrete]
GPU 2: AMD Radeon 610M [Integrated]
Memory: 16.31 GiB / 30.63 GiB (53%)
Swap: 79.35 MiB / 40.00 GiB (0%)
Disk (/): 1.66 TiB (20%) - btrfs

Compiled with
1. CMake Release Mode
2. gcc (GCC) 15.2.1 20260103
</pre>

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
- 支持异步日志
- 灵活的Target和Filter
- 十分灵活的可轻松注入的流式输出(默认提供了很多支持,比如大部分alib5模块以及glm)
- 丰富的manipulators
- 默认的Console通过log_tag实现了颜色输出(也支持自动检测是否写入文件从而自动关闭颜色输出),同时也支持File和RotateFile作为预制菜
- 实现自己的Target和Filter也相对简单,使用的是传统的虚函数OOP机制,而非模板
- Compact包提供比较灵活的表格输出
- 高度配置能力,无论是Logger的配置(缓存数量,patch大小,consumer数量,背压设置)还是LogFactory(level剪枝,默认额外信息配置),到每条消息(通过streamedcontext+特定的manipulator)都可以配置
- (实验性)也许你可以拿alib5::aout替代std::cout,但是由于日志流和常规输出流其实不太一样,所以可能有点别扭,因此为实验性

```cpp
#include <alib5/alogger.h>
#include <glm/glm.hpp>
#include <alib5/compact/manip_table.h>

using namespace alib5;

constexpr auto reset = LOG_COLOR3(None,None,None);
int main(){

    /// 三行代码,接近开箱即用的异步日志体验
    Logger logger;
    LogFactory lg(logger,"TestLogger");
    logger.append_mod<lot::Console>("console");
    // 比较原始的推送方式,使用std::format
    lg.log(Severity::Info,"Hello There!");
    lg.log(Severity::Info,"Hello {}!","There");
    
    // 舒服的方式:支持流式输出,流式输出不仅支持std::format支持的,还支持各种自定义类型
    // 见 log/manipulator.h 可以查看注入日志流的方式
    // 内置大量支持
    auto mat = glm::mat4(1.0);
    lg << "你好!" << endlog;
    lg(Severity::Info) << "我不好" << mat << endlog;

    // 默认提供的lot::Console支持颜色输出
    lg << LOG_COLOR1(Red) << "我的天啊" << endlog;
    lg << LOG_COLOR3(Yellow,None,Blink) << "真的可以吗" << endlog;

    // 支持大量manipulator,具体见manipulator.h
    lg << log_tfmt("{:.2f}") << 1.23456 << endlog; // 临时格式化
    lg << log_fmt("{:.2f}") << 1.234 << 2.345 << endlog; // 持久格式化(不建议使用,很容易fmt error)
    lg << log_source() << endlog; // 获取当前行的代码位置
    lg << log_tag(0,0) << endlog; // 塞入tag,事实上LOG_COLOR就是tag实现的
    log_rate::Info info;
    info.set_type(log_rate::Info::Rate).set_rate(10);
    lg << log_rate(info) << "限制输出速率(实验性)" << endlog; 
    lg << log_omit(mat,32) << endlog; // 限制数据显示长度
    lg << log_bin(glm::value_ptr(mat),16) << endlog; // 二进制输出(默认为Hex),可以与log_omit完美嵌套
    lg << "\n" << log_stacktrace() << endlog; // 输出栈信息
    
    // 以及重磅消息,表格输出,需要 compact/manip_table
    log_table table([](detail::Operator & op){
        // 稀疏数组存储,所以随意
        op[1][0] << LOG_COLOR1(Cyan) << "组件名字";
        op[1][1] << LOG_COLOR1(Cyan) << "组件名字";
        op[1][2] << LOG_COLOR1(Blue) << "ALogger";
        op[2][0] << "版本号";
        op[2][1] << LOG_COLOR1(Green) << 5 << "." << 0;
    });
    // 自动重置尾部
    table.add_restore_tag(reset);
    table.config = table.modern_dot();

    lg << "\n" << table << endlog;
    // 生来便支持嵌套表格
    log_table table2([&](detail::Operator & op){
        op[0][0] << LOG_COLOR1(Cyan) << "库";
        op[0][1] << LOG_COLOR1(Blue) << "aaaa0ggmcLib";
        op[1][0] << "ALogger";
        op[1][1] << table;
    });
    table2.add_restore_tag(reset);
    table2.config = table.unicode_rounded();
    lg << "\n" << table2 << endlog;

    /// 注意事项
    /// 你可能看到了内部表我没有使用样式设置,因为目前的字符串长度计算函数比较简单
    /// 只对中文在控制台的宽度进行了支持,因此遇上unicode边框便会出问题,不过你可以塞入自制的函数
    log_table my_func([](auto&){},[](std::string_view s)->StringCalcInfo{
        StringCalcInfo info;
        info.lines = {}; // 分行,建议按照\n 或者\r\n,否则显示很可能出问题
        info.cols = {}; // 每一行的宽度
        info.max_cols = 0; // 最大宽度,建议为max(cols),不然行为未定义,可能崩溃
        return info;
    });

    /// 更多feature见md
}
```
![运行截图][def]

## 数据处理库 [adata](./adata.md)
- 完整的pmr测试通过,说实话这个是整个aaaa0ggmcLib5唯一没有出现内存池逃逸的组件,虽然我也只测试了这个
- 目前支持读取&写入json,支持读取toml(toml提供了dump函数,但是调用会panic).数据处理使用策略模式,因此你可以自己实现一个新的读取器
- 默认提供的JSON在读取方面使用rapidjson SAX,速度很快,见(perf0);在写入方面支持自定义空白符,支持设置空白符密度,支持ensure_ascii,支持浮点精度,支持nan报错(不会终止,而是invoke errror)
- 支持类似python dict的语法能力,同时内部数据转换规则我认为适中
- 支持常见操作: merge diff prune
- 支持自赋值,自move: data = data["child"]; (注意隐式转换,见下面)
- 速度也不错,在我的测试平台下单层数据引用访问为10ns(见perf1)
- Drawback: 线程不安全,如果需要多线程自己上锁或其他

### 关于数据类型
- AData核心分为Object,Value,Array和Null
- Value又分为Int64,Double,Bool,String
- 其中Value内部数据支持隐式转换
- 但是AData大类型只支持Null到其他三个类型的互转,其他三个类型自己相互隐式转化会直接触发panic导致程序崩溃(注意,没有限制显式转换,隐式转换指的是operator= \[index\] \[name\] 之类的调用)
- 这个大类型转换的规则很严格,因此AData推出Validator机制利用自制简易DSL来根据schema对数据进行校验,并提供详细信息来保证数据就是你schema中想要的,Validator实际上功能类似Sanitizer,可以填充默认数据,目前能处理大部分需求

### 示例代码
```cpp
#include <alib5/alogger.h>
#include <alib5/adata.h>

using namespace alib5;

int main(){
    AData doc;
    // 基础类型无缝赋值 (支持 string_view-like, bool, int64, double)
    doc["name"] = "adata gen 5";
    doc["version"] = 5.0;
    doc["active"] = true;
    
    // 数组：自动扩展 (Auto-expand)，无需手动 resize
    doc["tags"][0] = "easy";
    doc["tags"][2] = "elegant"; // 索引 1 会自动处理
    
    // 对象：多级路径自动递归创建
    doc["meta"]["author"]["github"] = "aaaa0ggmc";

    // 安全性：严禁隐式 Cast。非法访问将触发 Panic (带栈输出的 abort)
    // doc = doc["meta"]; // 同类型 安全自赋值：OK
    // doc = doc["tags"]; // Object to Array? C++里不是这样的!!

    doc.load_from_memory(R"({"status": "ok" , "secret" : 1234 })");
    doc.dump_to_file("./config.json");

    // 深度集成：与 alogger 完美配合，高效序列化
    aout << doc << fls; 

    // 极致定制：通过 JSONConfig 实现精细控制
    data::JSONConfig cfg {
        .float_precision = 2,
        .sort_object = data::JSONConfig::sort_asc, // 字典序输出
        .filter = [](auto key, auto& node){ 
            using enum data::JSONConfig::FilterOp;
            return (key == "secret") ? Discard : Keep; // 动态过滤字段
        }
    };
    aout << doc.str(data::JSON(cfg)) << fls; // 缩写版 dump

    // 1. 定义 Schema：支持类型约束、必填项、默认值填充
    AData schema;
    schema.load_from_memory(R"({
        "name" : "REQUIRED TYPE STRING VALIDATE not_empty",
        "id"   : ["TYPE INT", 1024, "<- 自动填充默认值"],
        "arr"  : ["TYPE ARRAY MIN 1", ["TYPE INT"]] 
    })");
    Validator vl;
    vl.from_adata(schema);

    AData input; 
    input["name"].set_null(); 

    auto result = vl.validate(input);
    if(!result.success){
        aout << "Error: " << result.recorded_errors << fls;
    }
}
```
```txt
{
  "status" : "ok",
  "secret" : 1234
}
{
  "status" : "ok"
}
Error: [".name : Expected type String,got Null"]
```


### perf0 json读取速度测试
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
```

### perf1 单层引用测试
```cpp
AData d;
d["main"] = "Hello";
d["sub"][0] = 1;

aout << Benchmark([&d]{
    // 这里返回的是引用哦
    d["sub"][0].value().transform<int>() ++;
}).run(1000,1000).name("Release AData") << fls;
```
```txt
-----------------------
Release AData

TimeCost        :14.0007ms
RunTimes        :1000000
Average         :14.0007ns
ShortestAvgCall :11.873ns
LongestAvgCall  :32.057ns
Stddev          :1.85566e-06
CV              :13.25%
--------------------------------
```

## ECS系统 [aecs](./aecs.md)
- 比较简洁的api
- 相当多的注入方式
- 支持编译期的组件依赖以及处理
- (中性)使用Freelist策略(这意味着返回的aref是稳定的,但是也意味着可能出现空洞,不过位图尽量降低了遍历的成本)
```cpp
#include <alib5/aecs.h>
using namespace alib5::ecs;

struct Attributes{
    float mass;
};

struct Acceleration{
    using Dependency = ComponentStack<Attributes>;
    ref_t<Attributes> attr;

    float x;
    float y;

    // 注册绑定函数
    void bind_dep(Dependency::deptup_t& tup){
        auto [m_attr] = tup;
        attr = m_attr;
    }

    void add_force(float x,float y){
        this->x = x / attr->mass;
        this->y = y / attr->mass;
    }
};

struct Velocity{
    using Dependency = ComponentStack<Acceleration>;
    ref_t<Acceleration> accel;

    float x;
    float y;
    // 注册绑定函数
    void bind_dep(Dependency::deptup_t& tup){
        auto [m_accel] = tup;
        accel = m_accel;
    }
};

int main(){
    EntityManager manager;
    
    EntityWrapper a(manager);
    EntityWrapper b(manager);

    // 还算简单的获取添加方式,返回ref_t
    auto [refa_velo,refa_attr,refa_accel] = a.adds<Velocity,Attributes,Acceleration>();
    ref_t<Acceleration> refb_accel = b.add<Acceleration>();
    
    // 设置数值
    refa_attr->mass = 2.0;
    // 返回std::optional<ref_t>
    (*b.get<Attributes>())->mass = 1.0; 

    refa_accel->add_force(10, 10);
    refb_accel->add_force(10, 10);

    // 批量更新
    manager.update<Velocity>([](Velocity & v){
        v.x += v.accel->x;
        v.y += v.accel->y;
        std::cout << "Updated." << std::endl;
    });

    std::cout << refa_velo->x << " " << refa_velo->y << std::endl; 
}
```
```txt
Updated.
5 5
```

## 比较安全的引用系统 aref
- 其实就是容器+索引,只要保证容器不变索引没问题
- 那如果是容器也会变呢?那就保证容器的容器不变就行了,总有个东西不变的
- (滞后性)上面的这个MultiRefWrapper相比RefWrapper比较落后,refwrapper基本上重载了大部分符号转发给内部对象然而multiref并没有支持,同时multiref的[] () ++ --语义不是转发而是移动引用(主要是实战中使用场景为0,所以懒得跟进)

```cpp
#include <alib5/aref.h>
#include <alib5/alogger.h>
#include <vector>
using namespace alib5;

int main(){
    std::vector<int> vec = {1};
    aout << "Vec capacity😱:" << vec.capacity() << fls;
    auto ref0 = ref(vec,0);
    vec.reserve(vec.capacity() + 3);
    ref0.get() = 100;
    aout << vec << fls;

    std::vector<
        std::vector<int>
    > vec2 = {{1}};
    aout << "Vecs capacity😱:" << vec2.capacity() << fls;
    auto refs0 = refs(vec2,0,0);
    vec2.reserve(vec2.capacity() + 2);
    refs0.get() = 100;
    aout << vec2 << fls;
}
```
```txt
Vec capacity😱:1
[100]
Vecs capacity😱:1
[[100]]
```

## Rustic🦀的崩溃系统 adebug
- 什么都不说了兄弟,直接给你崩溃了
- alib5中panic的选择比较克制,一般都是致命错误,以及debug模式下不太能够忍受的错误
- 比如ArrayOutOfBounds,ForbiddenImplicitCast,ECS EntityInvalid却调用等等,涉及的一般都是如果我不panic就segmentfault的场景,不过AData::TOML::dump也有一个panic,因为dump还没实现
- 需要注意的是如果release模式下你没开生成debug symbol,那么panic得到的栈信息是空气
```cpp
// 这里作为测试案例覆盖一下,正常这个是std::abort()
#define ADEBUG_INTERNAL_PANIC_ABORT
#include <alib5/adebug.h>

int main(){
    bool im_bad = true;
    panic_if(im_bad,"Imma bad guy!");
    // 也支持格式化
    panicf_if(im_bad,"Imma bad {} boy!","bad");
    // 单纯的也支持
    auto fn = [&]{panic("Holy cow!");};
    fn();
    // 还有release模式下自动变成空的东西
    // 注意这个和assert不一样,这个相当于是panic_if,所以值为true的时候反而会panic!!
    panic_debug(true,"leihou");
}
```
```txt
Message  : Imma bad guy!
Condition: im_bad
Source   : ./Testing/main.cpp:7:5
Function : int main()
Stack    : 
  0# ./Testing/main.cpp:7 at main


Message  : Imma bad bad boy!
Source   : ./Testing/main.cpp:9:5
Function : int main()
Stack    : 
  0# ./Testing/main.cpp:9 at main


Message  : Holy cow!
Source   : ./Testing/main.cpp:11:19
Function : main()::<lambda()>
Stack    : 
  0# ./Testing/main.cpp:11 at operator()
  1# ./Testing/main.cpp:12 at main


Message  : leihou
Condition: true
Source   : ./Testing/main.cpp:15:5
Function : int main()
Stack    : 
  0# ./Testing/main.cpp:15 at main
```

## 性能测试 aperf
- 相信前面的文档你已经看出来了,用来测速的
- 测速的准确度我显然是不能保证的,ns级别测速可能会出现20%左右的浮动,但是us-ms级别还算准确
- 但是测试相对速度不就行了?
- 支持插入std::ostream(cout),alogger
- 同时对于alogger,compact包(compact/make_table.h)提供了一个不错的make_table更加美观
- 不过原本的格式还算美观了

```cpp
#include <alib5/aperf.h>
#include <alib5/adata.h>
#include <alib5/alogger.h>
#include <alib5/compact/make_table.h>
using namespace alib5;

int main(){
    AData d;
    d["howdy"] = 1;
    d["pity"] = 1.0f;

    // 第一个参数为单轮次数,第二个为轮次,单轮次数太少会导致CV偏大,轮次太少会导致CV偏小
    BenchmarkResults perf = Benchmark([&d]{
        d["howdy"].value().transform<int>()++;
    }).run(1000,100).name("DebugPerformance:INT");

    BenchmarkResults perf2 = Benchmark([&d]{
        d["pity"].value().transform<double>()++;
    }).run(1000,100).name("DebugPerformance:DOUBLE");

    std::cout << perf << std::endl;
    aout << make_table({perf,perf2},[](log_table & tb){
        // 默认是ascii格式+---+--+,我这里换成无缝unicode
        tb.config = tb.unicode_rounded();
        tb.config.col_align = ColAlign::Center;
        tb.config.row_align = RowAlign::Center;
    }) << fls;
}
```
```txt
-----------------------
DebugPerformance:INT

TimeCost        :4.67559ms
RunTimes        :100000
Average         :46.7559ns
ShortestAvgCall :45.117ns
LongestAvgCall  :55.803ns
Stddev          :1.94933e-06
CV              :4.169%
--------------------------------
╭─────────────────┬──────────────────────┬─────────────────────────╮
│      Test       │ DebugPerformance:INT │ DebugPerformance:DOUBLE │
├─────────────────┼──────────────────────┼─────────────────────────┤
│    TimeCost     │      4.675595ms      │       5.189702ms        │
├─────────────────┼──────────────────────┼─────────────────────────┤
│    RunTimes     │        100000        │         100000          │
├─────────────────┼──────────────────────┼─────────────────────────┤
│     Average     │     46.755950ns      │       51.897020ns       │
├─────────────────┼──────────────────────┼─────────────────────────┤
│ ShortestAvgCall │     45.117000ns      │       45.118000ns       │
├─────────────────┼──────────────────────┼─────────────────────────┤
│ LongestAvgCall  │     55.803000ns      │       72.426000ns       │
├─────────────────┼──────────────────────┼─────────────────────────┤
│     Stddev      │       0.000002       │        0.000007         │
├─────────────────┼──────────────────────┼─────────────────────────┤
│       CV        │        4.17%         │         12.56%          │
╰─────────────────┴──────────────────────┴─────────────────────────╯
```


## 算法库(实验性) aalgorithm
- 目前只是支持几个排序算法(插入,三种快排,冒泡)

```cpp
#include <alib5/aalgorithm.h>
#include <alib5/alogger.h>
#include <alib5/aperf.h>
#include <alib5/compact/make_table.h>

using namespace alib5;
using namespace alib5::algo;

int main(){
    std::vector<int> origin = {4,7,2,3,6,1,8,9,5,7,3,10,14,52,56,34,78,91};
    // 还算简单的api,其实很接近std::sort的说
    {
        auto a = origin;
        sort::bubble(origin.begin(), origin.end(), std::less<int>());
        aout << origin << fls;
    }
    // 支持inject
    {
        auto b = origin;
        // 这里的true表示反转
        // 需要注意的是sort先交换元素然后才检查inject,因此实际上swap_prev目前的数值是swap_aft以前的数值
        // (怎么感觉有点绕口令?) 
        // 不过本身这个就是对称操作
        // 注意inject调用次数不等于时间复杂度之类的,每个算法的inject的时机不尽相同
        aout << "Swaps For Insertion:\n";
        sort::insertion(b.data(),b.data() + b.size(), std::less<int>(),
        true,[&b](std::size_t swap_prev,std::size_t swap_aft){
            aout << b[swap_prev] << " <=> " << b[swap_aft] << ";";
        });
        aout << fls;
    }
    {
        auto b = origin;
        aout << "Swaps For Quick(MedianOfThree):\n";
        // 这个也是默认策略,因此可以不填写
        sort::quick<sort::PartitionMedianThree>(b.data(),b.data() + b.size(), std::less<int>(),
        true,[&b](std::size_t swap_prev,std::size_t swap_aft){
            aout << b[swap_prev] << " <=> " << b[swap_aft] << ";";
        });
        aout << fls;
    }

    /// 简单的性能计算,在我电脑Release下测试
    /// 注意是小数据情况测试
    aout << make_table({
        Benchmark([origin] mutable {
            sort::bubble(origin.begin(), origin.end(), std::less<int>());
        }).run(1000,100).name("Bubble Sort"),
        Benchmark([origin] mutable {
            sort::insertion(origin.begin(), origin.end(), std::less<int>());
        }).run(1000,100).name("Insertion Sort"),
        Benchmark([origin] mutable {
            sort::quick<sort::PartitionMedianThree>(origin.begin(), origin.end(), std::less<int>());
        }).run(1000,100).name("Quick Sort(Median of Three)"),
        Benchmark([origin] mutable {
            sort::quick<sort::PartitionHoare>(origin.begin(), origin.end(), std::less<int>());
        }).run(1000,100).name("Quick Sort(Hoare)"),
        Benchmark([origin] mutable {
            sort::quick<sort::PartitionLomuto>(origin.begin(), origin.end(), std::less<int>());
        }).run(1000,100).name("Quick Sort(Lomuto)")
    },[](log_table & tb){
        tb.config = tb.modern_dot();
    }) << fls;
}
```
![截图][algo]

## 简单的i18n支持 atranlator
- 基于AData JSON提供键值翻译功能,同时由于AData线程不安全因此提供FlattenTranslator达成O(1)访问 + 多线程安全的常量翻译器
- 支持传统的dot访问(a./b.c)与严谨的json pointer(/a/~1b/c)
- 支持读取文件夹,支持多个翻译文件(同种语言id)增量覆盖
- 对于文件夹扫描,按照ascii顺序(std::compare,a to z)进行依次读取

```cpp
#include <alib5/atranslator.h>
#include <alib5/alogger.h>
using namespace alib5;

int main(){
    Translator tl;
    // 如果是一个文件就读取文件,如果是文件夹读取文件夹内所有文件,第二个参数可传入函数对文件进行名字筛选
    // 默认只选择.json后缀文件
    // tl.load_from_entry(io::load_entry("./translation"));

    // 一个有效的翻译文件必须包含id和title,否则会因为校验不过返回false
    aout << "Result: " << tl.load_from_memory(R"({
        "id" : "zh_cn",
        "title" : "简体中文",
        "ok" : {
            "test" : "{}加载成功!"
        }
    })") << fls;
    // 增量覆盖
    aout << "Result: " << tl.load_from_memory(R"({
        "id" : "zh_cn",
        "title" : "简体中文",
        "ok" : {
            "test2" : "{}加载成功 * 2!"
        }
    })") << fls;
    // 默认会选择第一个加载的语言
    aout << "Switch: " << tl.switch_language("en_us") << fls; // 没有就会什么也不干
    // 需要注意的是Translator默认选择的是jsonp格式
    aout << tl.translate("/id") << fls;

    // 扁平化,返回optional<T>
    FlattenTranslator fl = *tl.flatten_dots();

    // 模板上的false表示不copy,除非你足够自信,还是建议选择默认的copy模式
    aout << fl.translate("ok.test", fl.translate<false>("title")) << fls;
    aout << fl.translate("ok.test2", fl.translate<false>("title")) << fls;
}
```
```txt
Result: true
Result: true
Switch: false
zh_cn
简体中文加载成功!
简体中文加载成功 * 2!
```

### 基础Translator和Flatten版本的对比
```cpp
// 保证公平使用jsonp
auto jsonp_fl = *tl.flatten_jsonp();
// 理论最小元
aout << Benchmark([&tl]{
    tl.translate<false>("/title");
}).run(1000, 100).name("Origin Base") << endlog;
aout << Benchmark([&jsonp_fl]{
    jsonp_fl.translate<false>("/title");
}).run(1000, 100).name("Flatten Base") << endlog;
// 复杂场景
aout << Benchmark([&tl]{
    tl.translate("/ok/test2", tl.translate("/title"));
}).run(1000, 100).name("Origin Complex") << endlog;
aout << Benchmark([&jsonp_fl]{
    jsonp_fl.translate("/ok/test2", jsonp_fl.translate("/title"));
}).run(1000, 100).name("Flatten Complex") << endlog;
```
```txt
-----------------------
Origin Base

TimeCost        :13.6694ms
RunTimes        :100000
Average         :136.694ns
ShortestAvgCall :128.228ns
LongestAvgCall  :157.911ns
Stddev          :5.24747e-06
CV              :3.839%
--------------------------------

-----------------------
Flatten Base

TimeCost        :439.303us
RunTimes        :100000
Average         :4.39303ns
ShortestAvgCall :3.561ns
LongestAvgCall  :5.937ns
Stddev          :6.16958e-07
CV              :14.04%
--------------------------------

-----------------------
Origin Complex

TimeCost        :37.2647ms
RunTimes        :100000
Average         :372.647ns
ShortestAvgCall :357.378ns
LongestAvgCall  :425.054ns
Stddev          :9.01576e-06
CV              :2.419%
--------------------------------

-----------------------
Flatten Complex

TimeCost        :6.65364ms
RunTimes        :100000
Average         :66.5364ns
ShortestAvgCall :60.552ns
LongestAvgCall  :87.86ns
Stddev          :3.5298e-06
CV              :5.305%
--------------------------------
```

[algo]: ./res/algo.png
[def]: ./res/alogger.png