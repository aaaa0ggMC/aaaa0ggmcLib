# 实体组件系统 (alib6.ecs)

`alib6.ecs` 是一套基于现代稀疏集（Sparse Set）架构的高性能 ECS 框架，提供零锁高效的多组件联合 View 查询、组件依赖声明与生命周期自动维护。

---

## 1. 核心架构设计

- **`Entity`**：由 32 位 ID 与 32 位版本代数（Version）组成的轻量句柄（64-bit）。
- **`ComponentPool<T>`**：基于密集数组（Dense Array）与稀疏映射（Sparse Map）的连续内存存储，保证缓存局部性（Cache Locality）。
- **`EntityManager`**：实体生命周期分配器与组件池管理器。
- **`View<Components...>`**：多组件联合查询视图，自动挑选最短密集列表进行交集遍历。

---

## 2. 快速上手示例

```cpp
import alib6;
#include <print>

using namespace alib6::ecs;

struct Position {
    float x{0.0f};
    float y{0.0f};
};

struct Velocity {
    float vx{0.0f};
    float vy{0.0f};
};

struct Health {
    int hp{100};
};

int main() {
    EntityManager em;

    // 1. 创建实体并挂载组件
    Entity player = em.create_entity();
    em.add_component<Position>(player, Position{0.0f, 0.0f});
    em.add_component<Velocity>(player, Velocity{1.5f, 2.0f});
    em.add_component<Health>(player, Health{100});

    Entity enemy = em.create_entity();
    em.add_component<Position>(enemy, Position{100.0f, 200.0f});
    em.add_component<Velocity>(enemy, Velocity{-0.5f, -0.5f});

    // 2. 物理移动系统：遍历同时拥有 Position 和 Velocity 的所有实体
    auto view = em.view<Position, Velocity>();
    view.each([](Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.vx;
        pos.y += vel.vy;
        std::println("实体 {} 新坐标: ({}, {})", e.id(), pos.x, pos.y);
    });

    // 3. 销毁实体与回收组件
    em.destroy_entity(enemy);
    assert(!em.is_valid(enemy));

    return 0;
}
```

---

## 3. 性能特征与内存优势

1. **密集内存迭代**：组件数据在 `ComponentPool` 中紧凑排列，CPU 缓存命中率极高。
2. **安全代数校验**：当实体被销毁后，其 ID 会被放入回收队列，并在复用时递增 `version`，杜绝野指针和悬垂句柄访问。
3. **PMR 隔离支持**：支持传入自定义内存资源，在关卡加载/卸载时实现整块内存瞬时释放。
