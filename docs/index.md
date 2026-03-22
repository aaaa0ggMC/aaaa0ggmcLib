# aaaa0ggmc's library gen5 文档

## 目录
- [aaaa0ggmc's library gen5 文档](#aaaa0ggmcs-library-gen5-文档)
  - [目录](#目录)
  - [前言](#前言)
  - [工具库 autil](#工具库-autil)
    - [Perf0 测速ext与std to\_string,release下](#perf0-测速ext与std-to_stringrelease下)

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
    
    /// 字符串转换,比std::to_string快一点点,但是能处理更多种类数据,见(perf0)
    // to_string 支持std::format能处理的类型
    std::string_view sv = ext::to_string<size_t,false>(10); // 第二个false表示不copy,返回string_view
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
    std::cout << Benchmark([]{
        auto sv = ext::to_string<double,false>(1.567890);
    }).run(10000,100).name("ext view") << std::endl;
    std::cout << Benchmark([]{
        auto cpy = ext::to_string(1.567890);
    }).run(10000,100).name("ext copy") << std::endl;
    std::cout << Benchmark([]{
        auto cpy = std::to_string(1.567890);
    }).run(10000,100).name("std copy") << std::endl;
}
```
```txt
-----------------------
ext view
TimeCost        :70.87835100000001ms
RunTimes        :1000000
Average         :70.87834819685668ns
ShortestAvgCall :65.89519999999999ns
LongestAvgCall  :99.02090000000001ns
Stddev          :5.4579075e-06
CV              :7.7%
--------------------------------
-----------------------
ext copy
TimeCost        :75.55038500000002ms
RunTimes        :1000000
Average         :75.55038610007614ns
ShortestAvgCall :71.8317ns
LongestAvgCall  :96.8839ns
Stddev          :3.3901188e-06
CV              :4.487%
--------------------------------
-----------------------
std copy

TimeCost        :37.93784800000001ms
RunTimes        :1000000
Average         :37.93784708250314ns
ShortestAvgCall :35.9752ns
LongestAvgCall  :43.9302ns
Stddev          :1.280802e-06
CV              :3.376%
--------------------------------
```

#include <alib5/aperf.h>
#include <alib5/alogger.h>
#include <alib5/compact/manip_table.h>

using namespace alib5;

int main(){
    LoggerConfig lcfg;
    lcfg.consumer_count = 0; // sync mode
    Logger logger(lcfg);
    LogFactoryConfig cfg;
    cfg.msg.disable_extra_information = true;
    LogFactory lg(logger,cfg);
    logger.append_mod<lot::Console>("console");

    lg << make_table(Benchmark([]{
        auto sv = ext::to_string<double,false>(1.567890);
    }).run(10000,100).name("ext view")) << endlog;
    lg << make_table(Benchmark([]{
        auto cpy = ext::to_string(1.567890);
    }).run(10000,100).name("ext copy")) << "\n" << endlog;
    
    lg << make_table(Benchmark([]{
        auto cpy = std::to_string(1.567890);
    }).run(10000,100).name("std copy")) << "\n" << endlog;
}

+-----------------+------------------------+
|      Test       |        ext view        |
+-----------------+------------------------+
|    TimeCost     |  101.67458300000004ms  |
+-----------------+------------------------+
|    RunTimes     |        1000000         |
+-----------------+------------------------+
|     Average     |  101.67458300000004ns  |
+-----------------+------------------------+
| ShortestAvgCall |  74.08770000000001ns   |
+-----------------+------------------------+
| LongestAvgCall  |  121.69839999999999ns  |
+-----------------+------------------------+
|     Stddev      | 1.4519253599993908e-05 |
+-----------------+------------------------+
|       CV        |         14.28%         |
+-----------------+------------------------+
+-----------------+------------------------+
|      Test       |        ext copy        |
+-----------------+------------------------+
|    TimeCost     |      80.946673ms       |
+-----------------+------------------------+
|    RunTimes     |        1000000         |
+-----------------+------------------------+
|     Average     |  80.94667299999999ns   |
+-----------------+------------------------+
| ShortestAvgCall |       71.8318ns        |
+-----------------+------------------------+
| LongestAvgCall  |       121.8172ns       |
+-----------------+------------------------+
|     Stddev      | 1.5205749983969996e-05 |
+-----------------+------------------------+
|       CV        |         18.78%         |
+-----------------+------------------------+

+-----------------+-----------------------+
|      Test       |       std copy        |
+-----------------+-----------------------+
|    TimeCost     |  38.47450099999998ms  |
+-----------------+-----------------------+
|    RunTimes     |        1000000        |
+-----------------+-----------------------+
|     Average     |  38.47450099999998ns  |
+-----------------+-----------------------+
| ShortestAvgCall |       36.6877ns       |
+-----------------+-----------------------+
| LongestAvgCall  |       40.2495ns       |
+-----------------+-----------------------+
|     Stddev      | 8.714122607918298e-07 |
+-----------------+-----------------------+
|       CV        |         2.26%         |
+-----------------+-----------------------+