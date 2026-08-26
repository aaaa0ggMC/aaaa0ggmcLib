import std;
import alib6;

auto main() -> int {
    std::println("=== alib6 Core Modules Comprehensive Test ===");

    // 1. 测试 str & ext
    std::string_view raw = "  hello \t world \n ";
    auto trimmed = alib6::str::trim(raw);
    std::println("[str::trim] '{}' -> '{}'", raw, trimmed);

    auto parts = alib6::str::split("apple,banana,orange", ",");
    std::println("[str::split] count: {}", parts.size());
    for (const auto& p : parts) {
        std::println("  - {}", p);
    }

    auto escaped = alib6::str::escape("Hello\nWorld\t\"Quote\"");
    auto unescaped = alib6::str::unescape(escaped);
    std::println("[str::escape]   {}", escaped);
    std::println("[str::unescape] {}", unescaped);

    int num = alib6::ext::to_T<int>("12345");
    bool flag = alib6::ext::to_T<bool>("true");
    std::println("[ext::to_T] parsed int: {}, bool: {}", num, flag);

    // 2. 测试 time
    auto cur_time = alib6::time::get_time();
    auto dur = alib6::time::format_duration(86400 * 2 + 3600 * 5 + 42);
    auto [norm_t, unit] = alib6::time::normalize_elapse(0.0052);
    std::println("[time::get_time] {}", cur_time);
    std::println("[time::format_duration] {}", dur);
    std::println("[time::normalize_elapse] {:.2f} {}", norm_t, unit);

    // 3. 测试 sys
    auto cpu = alib6::sys::get_cpu_brand();
    auto prog_mem = alib6::sys::get_prog_mem_usage();
    auto glob_mem = alib6::sys::get_global_mem_usage();
    std::println("[sys::get_cpu_brand] {}", cpu);
    std::println("[sys::prog_mem] RSS: {} KB", prog_mem.memory / 1024);
    std::println("[sys::glob_mem] Physical Total: {} MB, Used: {} MB ({}%)", 
        glob_mem.physical_total / (1024 * 1024), 
        glob_mem.physical_used / (1024 * 1024),
        glob_mem.percent
    );

    // 4. 测试 DeferManager
    {
        alib6::DeferManager dm;
        dm.defer([]() {
            std::println("[DeferManager] Step 1 executed on scope exit");
        });
        dm.defer([]() {
            std::println("[DeferManager] Step 2 executed on scope exit");
        });
        std::println("[DeferManager] Registered 2 deferred tasks");
    }

    // 5. 测试 Error & ErrorWrapper
    {
        alib6::Error err;
        auto simulate_work = [](int val, alib6::ErrorWrapper ew = {}) {
            if (val < 0) {
                ew.report(400, "Invalid value {}", val);
            }
        };

        simulate_work(-42, err);
        std::println("[Error] has_error: {}, error count: {}", err.has_error(), err.cursor);
        for (const auto& rec : err) {
            std::println("  - [Code {}] Message: '{}' at {}:{}", 
                rec.code, rec.message, rec.location.file_name(), rec.location.line());
        }
    }

    // 6. 测试 io 读写与遍历
    {
        std::string_view test_file = "test_io_temp.txt";
        alib6::io::write_all(test_file, "alib6 io test string 2026");
        auto content = alib6::io::read_all(test_file);
        std::println("[io::read_all] Content: '{}'", content);
        std::filesystem::remove(test_file);

        alib6::io::TraverseConfig cfg;
        cfg.depth = 1;
        auto traversed = alib6::io::traverse_files(".", cfg);
        std::println("[io::traverse_files] Found {} entries in current dir:", traversed.targets_absolute.size());
        for (const auto& entry : traversed) {
            std::println("  - {}", traversed.try_get_relative(entry));
        }
    }

    // 7. 测试 ref & refs 安全防悬垂容器引用与操作符穿透
    {
        std::vector<int> vec = {10, 20, 30};
        auto r = alib6::ref(vec, 1);
        std::println("[ref] initial *r: {}", *r);
        *r = 99;
        std::println("[ref] modified vec[1]: {}", vec[1]);

        // 测试扩容重分配后 ref 不悬垂
        vec.reserve(1000);
        vec.push_back(40);
        std::println("[ref after reallocation] *r: {}, index: {}", *r, r.get_index());

        // 测试显式内存偏移导航
        r.next_element();
        std::println("[ref::next_element] new *r: {}", *r);

        // 测试 operator[] 穿透 (引用容器中的 string / vector 元素)
        std::vector<std::string> str_vec = {"hello", "world"};
        auto str_ref = alib6::ref(str_vec, 0);
        std::println("[ref operator[] pass-through] str_ref[1]: '{}'", str_ref[1]); // 访问 "hello"[1] -> 'e'

        // 测试 operator() 穿透 (引用容器中的函数对象)
        std::vector<std::function<int(int, int)>> fn_vec = {
            [](int a, int b) { return a + b; },
            [](int a, int b) { return a * b; }
        };
        auto fn_ref = alib6::ref(fn_vec, 1);
        std::println("[ref operator() pass-through] fn_ref(6, 7): {}", fn_ref(6, 7)); // 穿透调用 lambda 乘法

        // 测试多层嵌套引用与显式层级导航 (parent, child, with_index)
        std::vector<std::vector<int>> matrix = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        };
        auto mr = alib6::refs(matrix, 1, 2);
        std::println("[refs] initial matrix[1][2]: {}", *mr);
        *mr = 666;
        std::println("[refs] updated matrix[1][2]: {}", matrix[1][2]);

        auto parent_ref = mr.parent(); // 显式回溯上一层
        std::println("[refs::parent] parent level size: {}", parent_ref->size());

        auto neighbor_ref = mr.with_index(0); // 显式横向切换同一层索引
        std::println("[refs::with_index] neighbor matrix[1][0]: {}", *neighbor_ref);
    }

    // 8. 测试 parser -> router -> command 现代化分发链
    {
        alib6::Command cmd;
        cmd.register_option({
            .name = "port",
            .short_name = "-p",
            .long_name = "--port",
            .description = "Server port",
            .default_val = "8080"
        });
        cmd.register_toggle({
            .name = "debug",
            .short_name = "-d",
            .long_name = "--debug",
            .description = "Debug mode"
        });

        cmd.add_route("service/{name}/start", [](const alib6::Command::CommandInput& in) {
            std::println("[Command::Handler] Starting service: '{}' on port: {}, debug: {}, worker: '{}'",
                in.key("name").view(),
                in.get<int>("port", 0),
                in.has("debug"),
                in.arg(0).value_or("default_worker")
            );
            return alib6::Command::CommandOutput::with_code(0);
        });

        cmd.from_str("service auth_cluster start -p 9090 -d worker_pool_0");
        std::println("[Command Help Output]:\n{}", cmd.help());
    }

    // 9. 测试 alib6.ecs 实体组件系统与 View 复合查询
    {
        struct Position { float x{0.0f}, y{0.0f}; };
        struct Velocity { float vx{0.0f}, vy{0.0f}; };
        struct NameTag : public alib6::ecs::IBindEntity {
            std::string label;
            NameTag(std::string l = "") : label(std::move(l)) {}
        };

        alib6::ecs::EntityManager em;

        auto e1 = em.create_entity();
        em.add_component<Position>(e1, 10.0f, 20.0f);
        em.add_component<Velocity>(e1, 1.5f, 2.5f);
        em.add_component<NameTag>(e1, "Player");

        auto e2 = em.create_entity();
        em.add_component<Position>(e2, 100.0f, 200.0f);
        em.add_component<Velocity>(e2, -5.0f, -5.0f);
        em.add_component<NameTag>(e2, "Enemy");

        std::println("[ECS] Created entities: {}, {}", e1, e2);

        // 使用 View 遍历
        em.view<Position, Velocity, NameTag>().for_each([](alib6::ecs::Entity ent, Position& p, Velocity& v, NameTag& tag) {
            p.x += v.vx;
            p.y += v.vy;
            std::println("  - [{}] {} new position: ({:.1f}, {:.1f})", ent, tag.label, p.x, p.y);
        });
    }

    // 10. alib6.log & aout Prefab 测试
    {
        alib6::aout << "[aout Prefab] Hello from alib6 logging system! Value: " << 42 << " Hex: " << alib6::log::log_tfmt("{:04x}") << 255 << alib6::log::endlog;
    }

    // 11. alib6.perf 基准测试
    {
        auto perf_res = alib6::perf::bench("VectorPushBackPerf", 100'000, 3, [] {
            std::vector<int> v;
            for (int i = 0; i < 10; ++i) v.push_back(i);
            alib6::perf::do_not_optimize(v);
        });
        std::println("{}", perf_res);
    }

    // 12. 测试 alib6.data 动态数据树、JSON、TOML 与 Schema 校验器
    {
        alib6::AData doc;
        doc["service_name"] = "AuthCluster";
        doc["port"] = 8080;
        doc["active"] = true;
        doc["metrics"]["cpu_limit"] = 4.0;
        doc["endpoints"][0] = "10.0.0.1";
        doc["endpoints"][1] = "10.0.0.2";

        // JSON 转储演示
        alib6::JSON json_engine;
        auto json_str = doc.dump_to_string(json_engine);
        std::println("[alib6.data JSON Dump]:\n{}", json_str);

        // TOML 转储演示
        alib6::TOML toml_engine;
        auto toml_str = doc.dump_to_string(toml_engine);
        std::println("[alib6.data TOML Dump]:\n{}", toml_str);

        // Schema 校验器与默认值注入演示
        alib6::AData schema;
        schema["service_name"] = "TYPE STRING REQUIRED";
        schema["port"] = "TYPE INT MIN 1024 MAX 65535 REQUIRED";
        schema["env"][0] = "TYPE STRING";
        schema["env"][1] = "production"; // 默认值

        alib6::Validator validator(schema);
        auto val_res = validator.validate(doc);
        std::println("[alib6.data Validator] Validate result: {}, Injected env default: '{}'",
            val_res.success ? "PASSED" : "FAILED",
            doc["env"].to<std::string_view>()
        );

        // 13. 测试 alib6.data:translator 国际化翻译与平铺快照
        alib6::Translator tr;
        tr.load_from_memory(R"({
            "id": "zh_cn",
            "title": "简体中文",
            "server": {
                "start": "服务器 '{}' 正在端口 {} 启动",
                "ready": "就绪"
            }
        })");
        auto trans_msg = tr.translate("/server/start", "AuthCluster", 8080);
        std::println("[alib6.data Translator]: {}", trans_msg);

        auto flat = tr.flatten_dots();
        if (flat) {
            std::println("[alib6.data FlattenTranslator]: server.ready -> '{}'", flat->get_key_value("server.ready"));
        }
        // 13. 测试 alib6.core:algo 排序与搜索
        std::vector<int> sort_sample = {42, 10, 88, 3, 55};
        alib6::algo::quick_sort(sort_sample);
        std::println("[alib6.core:algo QuickSort]: {} {} {} {} {}",
            sort_sample[0], sort_sample[1], sort_sample[2], sort_sample[3], sort_sample[4]);

        auto kmp_idx = alib6::algo::kmp_search(std::string_view("Hello modern alib6 world"), std::string_view("alib6"));
        std::println("[alib6.core:algo KMP]: Found 'alib6' at index {}", kmp_idx);
    }

    // 14. 测试 alib6.table 现代化表格与排版引擎
    {
        alib6::Table tbl(alib6::TableConfig::unicode_rounded());
        tbl[0][0] = "模块";
        tbl[0][1] = "功能描述";
        tbl[0][2] = "状态";

        tbl[1][0] = "alib6.core";
        tbl[1][1] = "核心 / PMR / Coroutine / Clock / Request / Algo";
        tbl[1][2] = "全部就绪 ✓";

        tbl[2][0] = "alib6.data";
        tbl[2][1] = "AData / JSON / TOML / Validator / Reflect / i18n";
        tbl[2][2] = "全部就绪 ✓";

        tbl[3][0] = "alib6.ecs";
        tbl[3][1] = "实体组件系统与 View 查询";
        tbl[3][2] = "全部就绪 ✓";

        tbl[4][0] = "alib6.log";
        tbl[4][1] = "PMR 多目标日志与标签管道";
        tbl[4][2] = "全部就绪 ✓";

        tbl[5][0] = "alib6.perf";
        tbl[5][1] = "统计基准测试与精度校验";
        tbl[5][2] = "全部就绪 ✓";

        tbl[6][0] = "alib6.table";
        tbl[6][1] = "多行/CJK/Emoji/全风格表格渲染";
        tbl[6][2] = "全部就绪 ✓";

        // 嵌套子表格演示
        alib6::Table sub_tbl(alib6::TableConfig::unicode_box());
        sub_tbl[0][0] = "嵌套项";
        sub_tbl[0][1] = "值";
        sub_tbl[1][0] = "子系统 A";
        sub_tbl[1][1] = "100%";
        sub_tbl[2][0] = "子系统 B";
        sub_tbl[2][1] = "100%";

        tbl[7][0] = "迁移状态";
        tbl[7][1] = sub_tbl;
        tbl[7][2] = "完成 🚀";

        std::println("[alib6.table Showcase]:\n{}", tbl);
    }

    std::println("=== All Tests Completed Successfully! ===");
    return 0;
}