# adata

## 目录
- [adata](#adata)
  - [目录](#目录)
  - [总纲](#总纲)
  - [当前Features与规划](#当前features与规划)
    - [Features](#features)
    - [规划](#规划)
  
## 总纲
AData是一个动态的数据与配置文件解决方案,具备类似动态语言如Python/Javascript等的灵活性与不错的安全性(具体的各种BUG我还没测试出来,需要漫长的维护期)与性能,同时支持数据校验.AData并不绑定一个数据存储形式(JSON,XML,TOML...)而是力图成为一个比较通用的数据解决方案,类似这些数据存储形式的IR.

## 当前Features与规划
### Features
<details>
  <summary>支持类似动态语言的食用方式</summary>

比如这里
```cpp
        constexpr double so_much = 1.0f;

    AData doc;
    // 基础类型,内部采用了缓存机制,不是纯纯std::string存储的
    /// 支持任何string_like(convertible_to_string_view)
    doc["name"] = "aaaa0ggmc's library generation 5";
    doc["version"] = "5.0";
    /// 支持bool
    doc["published"] = false;
    /// 支持整数,内部使用int64_t,所以面对uint64_t可能会溢出
    doc["nice"] = 1;
    /// 支持浮点数,内部采用double
    doc["thank you"] = so_much;
    /// Value类型丝滑转化
    doc["thank you"] = 2;
    
    // 数组,暂时不支持initializer_list哈,sorry啦
    /// 自动扩展(单次最大步长为conf_array_auto_expand=4)
    doc["features"][0] = "easy";
    doc["features"][1] = "visual";
    doc["features"][2] = "elegant"; 

    // 对象
    doc["good"]["what"] = 1;
    doc["good"]["can"] = 2;
    doc["good"]["i"] = 3;
    doc["good"]["do"] = 4;

    // 安全自赋值(这里doc和doc["good"]都是同类型可以赋值
    // 在adata中,隐式cast会panic(我不用异常,直接abort()+栈输出)) 
    doc = doc["good"];
```

</details>

<details>
    <summary>预制菜已经支持JSON</summary>

```cpp
    // 预制菜已经支持json
    doc.load_from_memory(R"({
        "this" : [ "is" , "nice" ]
    })");

    // 同样支持dump
    // 只要Dumper支持,target是什么都行!
    // doc.dump(T & target,Dumper && dumper = Dumper())
    /// 简单dump
    std::pmr::string val = doc.dump_to_string();
    val = doc.str(); // 缩写版本
    /// 写入文件
    doc.dump_to_file("./config");
    /// 配合alib5::io::FileEntry(见autil),这里false表示不需要文件一定存在
    doc.dump_to_entry(io::load_entry("./config",false));
    /// alogger完美集成JSON的dump,这里相当于自动调用了aout << doc.str() << fls,但是更加高效
    aout << doc << fls;
    /// 预制菜JSON便支持全面配置
    data::JSONConfig cfg;
    ///// 是否递归parse,默认true
    cfg.rapidjson_recursive = true;
    //// 行之间是否紧凑,有了这个输出类似 { "this" : [ "is","nice" ] }
    cfg.compact_lines = true;
    //// 空格是是紧凑的,有了这个类似 {"this":["is","nice"]}
    cfg.compact_spaces = true;
    //// 缩进可调整
    cfg.dump_indent = 2; 
    //// 缩进字符可调整
    cfg.dump_indent_char = '\t';
    //// 可以把unicode字符搞成\uxxxx \Uxxxxxxxx
    cfg.ensure_ascii = true;
    //// 遇到nan/inf的时候报警(返回值+invoke_error),不会影响dump只是提醒
    cfg.warn_when_nan = true;
    /// 浮点数格式化精度,小于0表示随意
    cfg.float_precision = 2;
    //// 排序
    cfg.sort_object = cfg.sort_asc;
    //// 过滤
    cfg.filter = [](auto key,auto& node){
        return (key == "bad") ? data::JSONConfig::Discard : data::JSONConfig::Keep;
    };

    std::cout << doc.str(data::JSON(cfg)) << std::endl;
```

</details>


### 规划
- 支持Acessor这层访问封装从而彻底地理论上支持任何数据类型而非目前的JSON
- (等C++26)支持静态反射绑定到类
- 加入一些有用的validates预制菜
- 提供对TOML的解析(因为老版本的ADATA就是提供了对JSON&TOML的解析的)


