#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <string>
#include <algorithm>
#include <chrono>
#include <format>
#include <thread>
#include <print>
#include <memory_resource>

import alib6;

using namespace alib6;

// 计算 R^2 判定系数
double calculate_r2(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) return 0.0;
    std::size_t n = x.size();
    
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0, sum_yy = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
        sum_yy += y[i] * y[i];
    }
    double numerator = (n * sum_xy - sum_x * sum_y);
    double denominator = (n * sum_xx - sum_x * sum_x) * (n * sum_yy - sum_y * sum_y);
    
    if (denominator <= 0.0) return 0.0; 
    return (numerator * numerator) / denominator; 
}

// 计算线性回归斜率
double calculate_slope(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    std::size_t n = x.size();
    
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
    }
    
    double denominator = (n * sum_xx - sum_x * sum_x);
    if (denominator == 0.0) return 0.0;
    
    return (n * sum_xy - sum_x * sum_y) / denominator;
}

struct ModelResult {
    std::string model_name;
    double r_squared;

    bool operator<(const ModelResult& other) const {
        return r_squared > other.r_squared; // 降序排序
    }
};

template<class SortFn>
void benchmark_sort_algo(std::string_view name, SortFn&& fn, std::size_t max_exp, std::size_t datasets = 3, std::size_t repeats = 3) {
    std::println("\n==========================================");
    std::println("[Benchmark: {}] Max Scale 2^{}, Datasets={}, Repeats={}", name, max_exp, datasets, repeats);
    std::println("==========================================");

    std::vector<double> sizes;
    std::vector<double> times_ms;

    std::random_device rd;
    std::mt19937_64 rng(rd());

    Clock total_clk;

    for (std::size_t exp = 1; exp <= max_exp; ++exp) {
        std::size_t n = 1ULL << exp;
        double sum_duration = 0.0;

        for (std::size_t d = 0; d < datasets; ++d) {
            std::vector<int> sample(n);
            std::uniform_int_distribution<int> dist(-1000000, 1000000);
            for (auto& x : sample) x = dist(rng);

            for (std::size_t r = 0; r < repeats; ++r) {
                auto copy_vec = sample;
                Clock clk;
                fn(copy_vec);
                sum_duration += clk.get_all();
            }
        }

        double avg_ms = sum_duration / (datasets * repeats);
        sizes.push_back(static_cast<double>(n));
        times_ms.push_back(avg_ms);

        std::println("  Scale 2^{:<2} (N={:<7}): {:>10.4f} ms", exp, n, avg_ms);
    }

    // 理论模型回归拟合
    std::vector<double> model_n, model_n_logn, model_n2, model_n3, model_logn;
    std::vector<double> log_sizes, log_times;

    for (std::size_t i = 0; i < sizes.size(); ++i) {
        double n = sizes[i];
        double t = std::max(times_ms[i], 1e-6);

        model_n.push_back(n);
        model_n_logn.push_back(n * std::log2(n));
        model_n2.push_back(n * n);
        model_n3.push_back(n * n * n);
        model_logn.push_back(std::log2(n));

        log_sizes.push_back(std::log(n));
        log_times.push_back(std::log(t));
    }

    std::vector<ModelResult> models = {
        {"O(N)", calculate_r2(model_n, times_ms)},
        {"O(N log N)", calculate_r2(model_n_logn, times_ms)},
        {"O(N^2)", calculate_r2(model_n2, times_ms)},
        {"O(N^3)", calculate_r2(model_n3, times_ms)},
        {"O(log N)", calculate_r2(model_logn, times_ms)},
    };

    std::sort(models.begin(), models.end());

    double power_a = calculate_slope(log_sizes, log_times);

    // 表格输出
    Table tbl(TableConfig::unicode_box());
    tbl[0][0] = "Model"; tbl[0][1] = "R^2";
    for (std::size_t i = 0; i < models.size(); ++i) {
        tbl[i + 1][0] = models[i].model_name;
        tbl[i + 1][1] = std::format("{:.6f}", models[i].r_squared);
    }
    tbl[models.size() + 1][0] = "N^a (Power-law)";
    tbl[models.size() + 1][1] = std::format("{:.4f}", power_a);
    tbl[models.size() + 2][0] = "Estimated Model";
    tbl[models.size() + 2][1] = models.front().model_name;

    std::println("拟合评估结果:\n{}", tbl);
}

int main() {
    std::println("=================================================================");
    std::println("       alib6 全量性能基准测试与各模块实测分析 (Release)           ");
    std::println("=================================================================");

    // 1. ext::to_string vs std::to_string
    {
        double val = 1234567890.12345;
        std::println("\n[1. ext::to_string vs std::to_string 基准测速]");
        auto r_ext_copy = Benchmark([&val]{ auto s = ext::to_string(val); }).run(10000, 100).name("alib6::ext::to_string (pmr::string)");
        auto r_std_copy = Benchmark([&val]{ auto s = std::to_string(val); }).run(10000, 100).name("std::to_string");

        std::println("{}", r_ext_copy);
        std::println("{}", r_std_copy);
    }

    // 2. AData 单层与多层引用性能
    {
        std::println("\n[2. AData 字典树与多层访问性能]");
        AData doc;
        doc["service"] = "AuthCluster";
        doc["metrics"]["limits"]["cpu"] = 4.0;
        doc["nodes"][0] = 100;

        auto r_single = Benchmark([&doc]{
            doc["service"].value().transform<std::string_view>();
        }).run(10000, 100).name("AData Single-Level Access");

        auto r_multi = Benchmark([&doc]{
            doc["metrics"]["limits"]["cpu"].value().transform<double>();
        }).run(10000, 100).name("AData 3-Level Nested Access");

        std::println("{}", r_single);
        std::println("{}", r_multi);
    }

    // 3. AData JSON 序列化与解析性能
    {
        std::println("\n[3. AData JSON 序列化与解析性能]");
        std::string json_str = R"({"name":"alib6","port":8080,"active":true,"workers":["w1","w2","w3"],"sub":{"key":42}})";
        AData parsed;

        auto r_parse = Benchmark([&]{
            parsed.load_from_memory(json_str, data::JSON{});
        }).run(5000, 50).name("AData JSON Parse");

        auto r_dump = Benchmark([&]{
            auto out = parsed.str(data::JSON{});
        }).run(5000, 50).name("AData JSON Dump");

        std::println("{}", r_parse);
        std::println("{}", r_dump);
    }

    // 4. ECS 10,000 实体 View 迭代性能
    {
        std::println("\n[4. ECS 10,000 实体多组件 View 迭代性能]");
        ecs::EntityManager em;
        struct Pos { float x{1.0f}, y{2.0f}; };
        struct Vel { float vx{0.1f}, vy{0.2f}; };

        for (int i = 0; i < 10000; ++i) {
            auto e = em.create_entity();
            em.add_component<Pos>(e, Pos{});
            em.add_component<Vel>(e, Vel{});
        }

        auto r_ecs = Benchmark([&]{
            auto v = em.view<Pos, Vel>();
            v.for_each([](ecs::Entity, Pos& p, Vel& v){
                p.x += v.vx;
                p.y += v.vy;
            });
        }).run(1000, 50).name("ECS 10k Entities 2-Component View Iteration");

        std::println("{}", r_ecs);
    }

    // 5. Logger 日志系统同步与异步后台队列吞吐测速
    {
        std::println("\n[5. alib6.log PMR 日志系统同步与异步模式吞吐测速]");
        
        using namespace alib6::log;

        struct NullTarget : public LogTarget {
            void write(LogMsg&) override {}
        };

        std::pmr::synchronized_pool_resource pool;

        // 5.1 同步直写模式 (consumer_count = 0)
        {
            Logger sync_logger(LoggerConfig{.consumer_count = 0}, &pool);
            sync_logger.append_mod<NullTarget>("null");
            LogFactory lg_sync(sync_logger, "SyncWorker");

            auto r_sync_basic = Benchmark([&]{
                lg_sync << "Worker " << 100 << " status: " << true << endlog;
            }).run(5000, 50).name("Logger Sync In-Thread Direct Output");

            std::println("{}", r_sync_basic);
        }

        // 5.2 异步后台线程队列模式 (consumer_count = 1)
        {
            Logger async_logger(LoggerConfig{.consumer_count = 1}, &pool);
            async_logger.append_mod<NullTarget>("null");
            LogFactory lg_async(async_logger, "AsyncWorker");

            auto r_async_basic = Benchmark([&]{
                lg_async << "Worker " << 100 << " status: " << true << endlog;
            }).run(5000, 50).name("Logger Async Single-Producer Output");

            auto r_async_tags = Benchmark([&]{
                lg_async << color(Color::Cyan) << "Highlighted message" << color(Color::None) << endlog;
            }).run(5000, 50).name("Logger Async Color Tag Output");

            auto r_async_source = Benchmark([&]{
                lg_async << log_source() << "Source location trace" << endlog;
            }).run(5000, 50).name("Logger Async Source Location Output");

            std::println("{}", r_async_basic);
            std::println("{}", r_async_tags);
            std::println("{}", r_async_source);
        }

        // 5.3 异步 4 线程高并发生产者吞吐测速
        {
            Logger multi_logger(LoggerConfig{.consumer_count = 2}, &pool);
            multi_logger.append_mod<NullTarget>("null");

            constexpr int thread_count = 4;
            constexpr int logs_per_thread = 100000;
            auto start_tp = std::chrono::high_resolution_clock::now();

            std::vector<std::jthread> workers;
            workers.reserve(thread_count);
            for (int t = 0; t < thread_count; ++t) {
                workers.emplace_back([&, t] {
                    LogFactory lg(multi_logger, "MultiWorker");
                    for (int i = 0; i < logs_per_thread; ++i) {
                        lg << "Worker #" << t << " event: " << i << endlog;
                    }
                });
            }
            workers.clear(); // 等待所有生产者线程发完
            multi_logger.flush(); // 等待消费者全部消化完

            auto end_tp = std::chrono::high_resolution_clock::now();
            auto total_time_ms = std::chrono::duration<double, std::milli>(end_tp - start_tp).count();
            int total_logs = thread_count * logs_per_thread;
            double ops = (total_logs / total_time_ms) * 1000.0;

            std::println("-----------------------");
            std::println("Logger Async Multi-Thread Throughput (4 Producers -> 2 Consumers)");
            std::println("Total Logs       : {}", total_logs);
            std::println("Total Time       : {:.2f} ms", total_time_ms);
            std::println("Throughput       : {:.2f} logs/sec ({:.2f} ns/log)", ops, (total_time_ms * 1e6) / total_logs);
            std::println("--------------------------------");
        }
    }

    // 6. 排序算法时间复杂度实测与回归分析
    std::println("\n=================================================================");
    std::println("           alib6.algo 各排序算法时间复杂度与性能评测              ");
    std::println("=================================================================");

    // O(N^2) 算法，测试到 2^13 (8192)
    benchmark_sort_algo("bubble_sort (冒泡排序)", [](auto& v){ algo::bubble_sort(v); }, 13);
    benchmark_sort_algo("cocktail_sort (鸡尾酒排序)", [](auto& v){ algo::cocktail_sort(v); }, 13);
    benchmark_sort_algo("gnome_sort (地精排序)", [](auto& v){ algo::gnome_sort(v); }, 13);
    benchmark_sort_algo("insertion_sort (插入排序)", [](auto& v){ algo::insertion_sort(v); }, 13);
    benchmark_sort_algo("odd_even_sort (奇偶排序)", [](auto& v){ algo::odd_even_sort(v); }, 13);
    benchmark_sort_algo("selection_sort (选择排序)", [](auto& v){ algo::selection_sort(v); }, 13);

    // O(N log N) / O(N^1.3) 算法，测试到 2^18 (262,144)
    benchmark_sort_algo("comb_sort (梳排序)", [](auto& v){ algo::comb_sort(v); }, 18);
    benchmark_sort_algo("shell_sort (希尔排序)", [](auto& v){ algo::shell_sort(v); }, 18);
    benchmark_sort_algo("quick_sort<Lomuto> (快排Lomuto)", [](auto& v){ algo::quick_sort<algo::PartitionLomuto>(v.begin(), v.end()); }, 18);
    benchmark_sort_algo("quick_sort<Hoare> (快排Hoare)", [](auto& v){ algo::quick_sort<algo::PartitionHoare>(v.begin(), v.end()); }, 18);
    benchmark_sort_algo("quick_sort<MedianThree> (快排三数取中)", [](auto& v){ algo::quick_sort<algo::PartitionMedianThree>(v.begin(), v.end()); }, 18);
    benchmark_sort_algo("std::sort (标准库内省排序)", [](auto& v){ std::sort(v.begin(), v.end()); }, 18);

    // 7. 搜索算法模式匹配测速 (KMP vs Plain Search)
    {
        std::println("\n=================================================================");
        std::println("        alib6.algo 搜索算法模式匹配基准测速 (1MB 文本)             ");
        std::println("=================================================================");

        std::string long_text(1024 * 1024, 'a');
        long_text[800000] = 'b';
        long_text[800001] = 'c';
        std::string pattern = "abc";

        algo::KmpContext kmp_ctx(pattern);
        auto r_kmp_ctx = Benchmark([&]{
            auto idx = algo::kmp_search(long_text, kmp_ctx);
            (void)idx;
        }).run(500, 20).name("KMP Search with Precomputed KmpContext (1MB text)");

        auto r_kmp = Benchmark([&]{
            auto idx = algo::kmp_search(long_text, pattern);
            (void)idx;
        }).run(500, 20).name("KMP Search On-the-Fly (1MB text)");

        auto r_plain = Benchmark([&]{
            auto idx = algo::plain_search(long_text, pattern);
            (void)idx;
        }).run(500, 20).name("Plain Search (1MB text)");

        std::println("{}", r_kmp_ctx);
        std::println("{}", r_kmp);
        std::println("{}", r_plain);

        std::println("\n[最坏恶劣对抗数据测试 (Adversarial Data)]: 100k 'a's, pattern: 200 'a's + 'b'");
        std::string adv_text(100000, 'a');
        std::string adv_pat(200, 'a');
        adv_pat.push_back('b');

        auto r_kmp_adv = Benchmark([&]{
            auto idx = algo::kmp_search(adv_text, adv_pat);
            (void)idx;
        }).run(500, 20).name("KMP Search (Adversarial)");

        auto r_plain_adv = Benchmark([&]{
            auto idx = algo::plain_search(adv_text, adv_pat);
            (void)idx;
        }).run(500, 20).name("Plain Search (Adversarial)");

        std::println("{}", r_kmp_adv);
        std::println("{}", r_plain_adv);
    }

    std::println("\n=== All Benchmarks Completed Successfully! ===");
    return 0;
}
