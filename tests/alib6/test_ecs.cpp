/**
 * @file test_ecs.cpp
 * @brief alib6.ecs 模块完整功能与 PMR 内存隔离单测
 */
#include <gtest/gtest.h>
#include "pmr_tracker.h"
import std;
import alib6;

using namespace alib6::ecs;

// 简单测试组件
struct Position {
    float x{0.0f};
    float y{0.0f};
};

struct Velocity {
    float vx{0.0f};
    float vy{0.0f};
};

// 具备生命周期 cleanup 与 bind 注入的测试组件
struct CleanableTag : public IBindEntity, public ISlotComponent {
    std::string name;
    static inline int cleanup_count{0};

    CleanableTag(std::string n = "") : name(std::move(n)) {}

    void cleanup() {
        ++cleanup_count;
        name.clear();
    }
};

// 具备依赖注入特性的测试组件
struct PhysicsBody {
    using Dependency = ComponentStack<Position, Velocity>;

    float mass{1.0f};
    bool bound_deps{false};

    void bind_dep(typename Dependency::deptup_t& deps) {
        bound_deps = true;
        // 验证依赖元组已正确填充
        auto& pos_ref = alib6::ecs::get<Position>(deps);
        auto& vel_ref = alib6::ecs::get<Velocity>(deps);
        EXPECT_TRUE(pos_ref.valid());
        EXPECT_TRUE(vel_ref.valid());
    }
};

// 具备 update 方法的组件
struct Particle {
    float life{100.0f};

    void update(float dt) {
        life -= dt;
    }
};

// ==================== 基础实体测试 ====================
TEST(ECSTest, EntityBasicsAndFormatting) {
    Entity e1(1, 0);
    Entity e2(1, 0);
    Entity e3(2, 0);
    Entity null_e = Entity::null();

    EXPECT_EQ(e1, e2);
    EXPECT_NE(e1, e3);
    EXPECT_TRUE(e1.is_valid());
    EXPECT_FALSE(null_e.is_valid());
    EXPECT_TRUE(null_e.is_null());

    // 测试 std::hash
    std::hash<Entity> hasher;
    EXPECT_EQ(hasher(e1), hasher(e2));
    EXPECT_NE(hasher(e1), hasher(e3));

    // 测试 std::format
    std::string fmt_str = std::format("{}", e1);
    EXPECT_EQ(fmt_str, "Entity(id: 1, v: 0)");

    std::string null_fmt = std::format("{}", null_e);
    EXPECT_EQ(null_fmt, "Entity(null)");
}

// ==================== 实体管理与代际复用 ====================
TEST(ECSTest, EntityManagerLifecycleAndSlotReuse) {
    EntityManager em;
    EXPECT_EQ(em.entity_count(), 0);

    Entity e1 = em.create_entity();
    EXPECT_EQ(e1.id, 1);
    EXPECT_EQ(e1.version, 0);
    EXPECT_TRUE(em.is_valid(e1));

    Entity e2 = em.create_entity();
    EXPECT_EQ(e2.id, 2);
    EXPECT_EQ(em.entity_count(), 2);

    // 销毁 e1
    em.destroy_entity(e1);
    EXPECT_FALSE(em.is_valid(e1));
    EXPECT_EQ(em.entity_count(), 1);

    // 再次创建实体，应当复用槽位 1 但版本号递增至 1
    Entity e1_reused = em.create_entity();
    EXPECT_EQ(e1_reused.id, 1);
    EXPECT_EQ(e1_reused.version, 1);
    EXPECT_TRUE(em.is_valid(e1_reused));
    EXPECT_FALSE(em.is_valid(e1)); // 旧 handle 失效
    EXPECT_EQ(em.entity_count(), 2);
}

// ==================== 组件挂载、获取与清理钩子 ====================
TEST(ECSTest, ComponentAddGetAndCleanupHook) {
    CleanableTag::cleanup_count = 0;
    EntityManager em;

    Entity e = em.create_entity();

    // 挂载 Position
    auto pos_ref = em.add_component<Position>(e, 10.0f, 20.0f);
    EXPECT_EQ(pos_ref->x, 10.0f);
    EXPECT_EQ(pos_ref->y, 20.0f);

    // 挂载带 IBindEntity 与 ISlotComponent 的 CleanableTag
    auto tag_ref = em.add_component<CleanableTag>(e, "TestEntity");
    EXPECT_EQ(tag_ref->name, "TestEntity");
    EXPECT_EQ(tag_ref->get_bound(), e);
    EXPECT_EQ(tag_ref->get_slot(), 0);

    // 查询组件
    EXPECT_TRUE(em.has_component<Position>(e));
    EXPECT_TRUE(em.has_component<CleanableTag>(e));
    EXPECT_FALSE(em.has_component<Velocity>(e));

    auto retrieved_pos = em.get_component<Position>(e);
    ASSERT_TRUE(retrieved_pos.has_value());
    EXPECT_EQ(retrieved_pos->get().x, 10.0f);

    // 操作符穿透修改
    retrieved_pos.value()->x = 99.0f;
    EXPECT_EQ(em.get_component_raw<Position>(e)->x, 99.0f);

    // 移除 CleanableTag，触发 cleanup 钩子并解绑 Entity
    auto res = em.remove_component<CleanableTag>(e);
    EXPECT_EQ(res, EntityManager::DRSuccess);
    EXPECT_EQ(CleanableTag::cleanup_count, 1);
    EXPECT_FALSE(em.has_component<CleanableTag>(e));

    // 销毁整个实体，组件级联回收
    em.destroy_entity(e);
    EXPECT_FALSE(em.is_valid(e));
}

// ==================== 依赖注入与递归依赖解析 ====================
TEST(ECSTest, ComponentDependencyInjection) {
    EntityManager em;
    Entity e = em.create_entity();

    // 仅添加 PhysicsBody，EntityManager 会自动递归挂载 Position 和 Velocity
    auto pb_ref = em.add_component<PhysicsBody>(e, 5.5f);
    EXPECT_EQ(pb_ref->mass, 5.5f);
    EXPECT_TRUE(pb_ref->bound_deps);

    // 验证依赖的组件已被隐式自动挂载
    EXPECT_TRUE(em.has_component<Position>(e));
    EXPECT_TRUE(em.has_component<Velocity>(e));
    EXPECT_TRUE(em.has_component<PhysicsBody>(e));
}

// ==================== 多组件 View 复合查询 ====================
TEST(ECSTest, MultiComponentViewIteration) {
    EntityManager em;

    // Entity 1: Pos + Vel
    Entity e1 = em.create_entity();
    em.add_component<Position>(e1, 1.0f, 2.0f);
    em.add_component<Velocity>(e1, 10.0f, 20.0f);

    // Entity 2: Pos only
    Entity e2 = em.create_entity();
    em.add_component<Position>(e2, 5.0f, 6.0f);

    // Entity 3: Pos + Vel
    Entity e3 = em.create_entity();
    em.add_component<Position>(e3, 100.0f, 200.0f);
    em.add_component<Velocity>(e3, 10.0f, 10.0f);

    // 1. 测试 view.for_each(func(Entity, Pos&, Vel&))
    auto view = em.view<Position, Velocity>();
    EXPECT_EQ(view.count(), 2);
    EXPECT_FALSE(view.empty());

    int count = 0;
    view.for_each([&](Entity ent, Position& p, Velocity& v) {
        ++count;
        p.x += v.vx;
        p.y += v.vy;
    });
    EXPECT_EQ(count, 2);

    // 验证位置更新
    EXPECT_EQ(em.get_component_raw<Position>(e1)->x, 11.0f);
    EXPECT_EQ(em.get_component_raw<Position>(e1)->y, 22.0f);
    EXPECT_EQ(em.get_component_raw<Position>(e3)->x, 110.0f);
    EXPECT_EQ(em.get_component_raw<Position>(e3)->y, 210.0f);
    EXPECT_EQ(em.get_component_raw<Position>(e2)->x, 5.0f); // 未被修改

    // 2. 测试基于迭代器的结构化绑定
    int iter_count = 0;
    for (auto [ent, p, v] : em.view<Position, Velocity>()) {
        ++iter_count;
        EXPECT_TRUE(ent == e1 || ent == e3);
    }
    EXPECT_EQ(iter_count, 2);
}

// ==================== EntityWrapper 句柄封装 ====================
TEST(ECSTest, EntityWrapperConvenience) {
    EntityManager em;

    auto wrap = em.create_wrapper();
    EXPECT_TRUE(wrap.is_valid());
    EXPECT_FALSE(wrap.is_null());

    wrap.add<Position>(1.0f, 1.0f);
    EXPECT_TRUE(wrap.has<Position>());

    auto raw_pos = wrap.get_raw<Position>();
    ASSERT_NE(raw_pos, nullptr);
    EXPECT_EQ(raw_pos->x, 1.0f);

    auto ref_pos = wrap.get<Position>();
    ASSERT_TRUE(ref_pos.has_value());
    ref_pos->get().y = 5.0f;
    EXPECT_EQ(raw_pos->y, 5.0f);

    wrap.destroy();
    EXPECT_FALSE(wrap.is_valid());
    EXPECT_TRUE(wrap.is_null());
}

// ==================== 单类型 update 派发 ====================
TEST(ECSTest, UniformUpdateDispatch) {
    EntityManager em;

    Entity e1 = em.create_entity();
    Entity e2 = em.create_entity();

    em.add_component<Particle>(e1, 100.0f);
    em.add_component<Particle>(e2, 50.0f);

    // 派发 update(dt)
    em.update<Particle>(10.0f);

    EXPECT_EQ(em.get_component_raw<Particle>(e1)->life, 90.0f);
    EXPECT_EQ(em.get_component_raw<Particle>(e2)->life, 40.0f);
}

// ==================== ComponentTraits 特征萃取与日志输出 ====================
TEST(ECSTest, ComponentTraitsAndLogging) {
    EXPECT_TRUE(ComponentTraits<CleanableTag>::cleanup);
    EXPECT_TRUE(ComponentTraits<CleanableTag>::bind);
    EXPECT_TRUE(ComponentTraits<CleanableTag>::slot_id);
    EXPECT_TRUE(ComponentTraits<PhysicsBody>::dependency);
    EXPECT_TRUE(ComponentTraits<PhysicsBody>::bind_dependency);

    // 测试 write_to_log
    std::string log_buf;
    ComponentTraits<CleanableTag>::write_to_log(log_buf);
    EXPECT_FALSE(log_buf.empty());
    EXPECT_TRUE(log_buf.find("Cleanup") != std::string::npos);

    // 测试 check
    std::stringstream ss;
    ComponentTraits<PhysicsBody>::check(ss);
    std::string ss_str = ss.str();
    EXPECT_FALSE(ss_str.empty());
    EXPECT_TRUE(ss_str.find("Dependency") != std::string::npos);

    // 测试 summary
    auto sum = ComponentTraits<Position>::summary();
    EXPECT_FALSE(sum.empty());
}

// ==================== PMR 内存池隔离与泄漏检测 Section ====================
TEST(ECSTest, PMRMemoryIsolationAndLeakCheck) {
    alib6::test::CountingMemoryResource tracker;

    {
        EntityManager em(32, 32, &tracker);
        EXPECT_GT(tracker.allocated_bytes(), 0);

        for (int i = 0; i < 50; ++i) {
            Entity e = em.create_entity();
            em.add_component<Position>(e, static_cast<float>(i), static_cast<float>(i * 2));
            em.add_component<CleanableTag>(e, "PMR Test Long String Component Content...");
            if (i % 2 == 0) {
                em.add_component<Velocity>(e, 1.0f, 1.0f);
            }
        }

        EXPECT_EQ(em.entity_count(), 50);

        // 销毁一部分实体
        for (int i = 1; i <= 20; ++i) {
            Entity e(i, 0);
            em.destroy_entity(e);
        }
        EXPECT_EQ(em.entity_count(), 30);

        // 复用创建新实体
        for (int i = 0; i < 10; ++i) {
            Entity e = em.create_entity();
            em.add_component<Position>(e, 999.0f, 999.0f);
        }

        // 触发 View 遍历
        em.view<Position, Velocity>().for_each([](Entity ent, Position& p, Velocity& v) {
            p.x += v.vx;
        });
    }

    // 作用域析构后，EntityManager、内部组件池与映射表占用的全部内存必须 100% 归还，绝无泄漏
    EXPECT_FALSE(tracker.has_leak());
    EXPECT_EQ(tracker.live_bytes(), 0);
}
