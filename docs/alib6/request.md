# 批量窃取请求队列 (alib6.request)

`alib6.request` 是一套面向高吞吐多线程场景的多生产者/多消费者（MPMC）批量窃取请求队列管理器。

---

## 1. 设计原理

- **双队列原子窃取（Batch Stealing）**：
  - 生产者队列（`request_list_pending`）与消费者队列（`request_list_consumer`）完全解耦。
  - 生产者仅对 `producer_mutex` 上锁并按照批次边界（`conf_steal_batch_size = 32`）唤醒消费者。
  - 当消费者队列变空时，通过原子 `swap` 瞬间窃取整批生产者请求，极大减轻多线程锁争用（Lock Contention）。
- **`std::visit` 零虚表派发**：
  - 基于 C++ 泛型可变参 `std::variant<std::monostate, RequestTypes...>` 进行请求保存与直接分发。
- **全链路 PMR 内存池注入**：
  - 内部 `std::pmr::list` 与 `std::pmr::deque` 全走注入的内存资源。

---

## 2. 代码示例

```cpp
import alib6;
#include <print>

using namespace alib6;

struct AppContext {
    int processed_jobs{0};
};

struct PrintRequest {
    std::string message;

    void handle_request(AppContext& ctx) {
        std::println("处理请求: {}", message);
        ++ctx.processed_jobs;
    }
};

struct ComputeRequest {
    int a;
    int b;

    void handle_request(AppContext& ctx) {
        std::println("计算结果: {} + {} = {}", a, b, a + b);
        ++ctx.processed_jobs;
    }
};

int main() {
    RequestManager<AppContext, PrintRequest, ComputeRequest> mgr;
    AppContext ctx;

    // 生产者线程投递任务
    mgr.push_request<PrintRequest>("Hello alib6 MPMC queue!");
    mgr.push_request<ComputeRequest>(100, 200);

    // 消费者窃取并批量执行
    std::size_t count = mgr.process_batch(ctx);
    std::println("成功处理 {} 个请求，当前累计执行 {} 个任务", count, ctx.processed_jobs);

    return 0;
}
```
