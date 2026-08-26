# 核心工具库 (alib6.core)

`alib6.core` 是整套类库的基石，聚合了 PMR 内存基础设施、错误处理、安全引用、基础数据结构、IO 遍历、命令行解析与动态路由等关键功能。

---

## 1. 错误处理与 Panic 系统 (`:error` & `:debug`)

### 1.1 `ErrorWrapper` 零开销注入
`alib6` 弃用繁重缓慢的异常机制，在可能出错的 API 末尾统一提供 `ErrorWrapper err = {}` 参数。当调用方不关心错误时，编译器常量折叠可达到 **0 额外分支与分配开销**；当需要捕获错误时，自动收集错误码与调用现场的 `std::source_location`。

```cpp
import alib6;
#include <print>

using namespace alib6;

void parse_port(std::string_view str, ErrorWrapper err = {}) {
    auto [val, ok] = ext::to_T<int>(str);
    if (!ok || val < 0 || val > 65535) {
        err.report(400, "Port must be in range 0-65535");
    }
}

int main() {
    Error err;
    parse_port("99999", &err);
    if (err.has_error()) {
        std::println("捕获错误: {} (触发位置: {}:{})", 
            err.latest_error().msg,
            err.latest_error().loc.file_name(),
            err.latest_error().loc.line());
    }
}
```

### 1.2 Rustic 风格的硬核防御 (`:debug`)
提供自动捕获堆栈回溯的 `panic` 与 `panic_if`：
```cpp
panic_if(ptr == nullptr, "Null pointer detected!");
panicf_if(val < 0, "Invalid negative index: {}", val);
```

---

## 2. 容器安全引用系统 (`:ref`)

在 C++ 中，当 `std::vector` 等容器发生扩容重分配（Reallocation）时，普通的原始指针或迭代器会立即失效并导致悬垂 UB。
`alib6.core:ref` 提供了基于 **容器指针 + 动态索引** 的双层包装机制（`RefWrapper` 与 `MultiRefWrapper`），并实现 **100% 透明操作符穿透**：

```cpp
#include <vector>
#include <print>
import alib6;

using namespace alib6;

int main() {
    std::vector<int> vec = {10, 20, 30};
    auto r = ref(vec, 1); // 引用 vec[1] (当前值为 20)

    // 触发容器扩容重分配！
    vec.reserve(1000);
    vec.push_back(40);

    // 操作符完全穿透至底层元素
    *r = 99; // vec[1] 变为 99，安全无虞！
    std::println("vec[1] = {}", vec[1]); // 99

    // 显式索引导航
    r.next_element(); // 步进至 vec[2]
    std::println("*r after next_element = {}", *r); // 30
}
```

---

## 3. 管道解析器与动态路由器 (`:parser` & `:router`)

### 3.1 管道式流式解析 (`Parser` & `Analyser`)
```cpp
Parser p("key = 12345 ; other = abc");
auto kv = p.pipe()
           .read_until('=')
           .skip_chars(" =")
           .read_until(';')
           .finish();
```

### 3.2 动态路由器 (`Router`)
支持树形动态路由匹配与通配符捕获：
```cpp
Router<std::string> router;
router.add_route("/api/v1/user/{id}", "UserInfoHandler");
router.add_route("/api/v1/static/*", "StaticFileHandler");

auto match = router.match("/api/v1/user/42");
if (match.has_value()) {
    std::println("匹配到处理器: {}, 捕获参数 id: {}", 
        match->handler, match->params["id"]);
}
```

---

## 4. 现代化命令行解析引擎 (`:cmd`)

支持声明式选项、开关、位置参数及自动生成精美 ASCII/Unicode 树形帮助菜单：

```cpp
Command cmd("service", "Microservice runner");
cmd.add_positional("name", "Name of service");
cmd.add_option("-p", "--port", "Server port", "8080");
cmd.add_toggle("-d", "--debug", "Enable debug mode");

cmd.set_handler([](const CommandContext& ctx) {
    std::println("Starting {} on port {}, debug: {}", 
        ctx.get_positional("name"),
        ctx.get_option("--port"),
        ctx.has_toggle("--debug"));
});

// 打印帮助文档
std::println("{}", cmd.format_help());
```

---

## 5. 高性能存储原语 (`:storage`)

- **`MonoBitSet`**：支持动态扩容的高效单调位图，快速 `test()`、`set()`、`reset()` 与 `find_first_unset()`。
- **`FreeList<T>`**：带空闲链表与复用槽位的对象池，元素索引永不改变，极其适合游戏对象管理。
