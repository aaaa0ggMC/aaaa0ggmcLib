# 协程与时钟调度系统 (alib6.co & alib6.clock)

`alib6.co` 与 `alib6.clock` 提供了现代 C++23 协程任务抽象、线程同步原语与高精度自适应帧率调度控制。

---

## 1. 协程任务与驱动 (`alib6.co:task`)

基于 C++23 `std::generator<T>` 封装了具备确定性单步执行能力的 `Task<T>`：

```cpp
import alib6;
#include <print>

using namespace alib6;

std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        int tmp = a + b;
        a = b;
        b = tmp;
    }
}

int main() {
    co::Task task(fibonacci());

    for (int i = 0; i < 6; ++i) {
        task.next();
        std::println("Fibonacci: {}", *task.current.value());
    }
}
```

---

## 2. 并发同步原语 (`alib6.co:sync`)

- **`Signal`**：原子事件通知信号，`fire()` 唤醒，`ready()`/`until()` 判定。
- **`WaitGroup`**：支持 RAII `make_guard()` 的原子任务计数同步器。
- **`Race<Vs...>`**：多竞态条件监听器，记录率先就绪的事件索引。

```cpp
co::Signal sig;
co::WaitGroup wg;

std::jthread worker([guard = wg.make_guard(), &sig] {
    // 执行异步任务...
    Timer(10.0).wait();
    sig.fire();
});

// 在当前线程驱动协程任务直到信号触发
auto gen = nap0(); // 让出 CPU
co::Task waiter(std::move(gen));
co::wait_until(waiter, sig);

assert(sig.ready());
```

---

## 3. 高精度时钟与 FPS 限制器 (`alib6.clock`)

### 3.1 高精度分段时钟 (`Clock`)
支持毫秒/微秒级 `start()`, `pause()`, `resume()`, `stop()`, `reset()` 及单帧增量 `get_offset()` 提取。

### 3.2 自适应帧率限制器 (`RateLimiter`)
适用于游戏主循环与渲染器，支持微秒级动态补偿与协程任务调度：

```cpp
RateLimiter fps_limiter(60.0); // 锁定 60 FPS
Clock frame_clock;

while (running) {
    frame_clock.clear_offset();

    // 执行渲染逻辑...
    render_scene();

    // 自适应补偿休眠并等待下一帧
    fps_limiter.wait();
}
```
