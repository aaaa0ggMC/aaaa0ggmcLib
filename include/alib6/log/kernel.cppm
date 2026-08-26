/**
 * @file kernel.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 日志核心系统 (Logger 消息队列与多线程消费引擎、LogFactory 流式句柄工厂)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>

export module alib6.log:kernel;
import std;
import alib6.core;
import :config;
import :msg;
import :mod;
import :manip;
import :targets;
import :filters;
import :stream;

namespace pmr = std::pmr;

export namespace alib6::log {

    class LogFactory;

    /**
     * @brief 日志系统核心引擎
     * 负责日志模块管理 (LogTarget / LogFilter)、生产者消息队列调度、PMR 内存池维护及后台工作线程分发。
     */
    struct Logger {
        using targets_t = pmr::deque<std::shared_ptr<LogTarget>>;
        using filters_t = pmr::deque<std::shared_ptr<LogFilter>>;

        friend class LogFactory;

    private:
        LoggerConfig config;
        memory_resource* mem_res{nullptr};

        std::mutex mod_lock;
        targets_t targets;
        pmr::unordered_map<pmr::string, RefWrapper<targets_t>> search_targets;
        filters_t filters;
        pmr::unordered_map<pmr::string, RefWrapper<filters_t>> search_filters;

        std::mutex msg_lock;
        std::condition_variable cv;
        std::condition_variable cv_flush;
        std::vector<std::jthread> consumers;

        std::mutex header_pool_lock;
        pmr::unordered_set<pmr::string> header_pool;

        pmr::deque<LogMsg> messages;
        std::atomic<usize> message_size{0};
        std::atomic<usize> active_consumers{0};

        usize back_pressure_threshold{0};
        std::atomic<bool> logger_running{true};
        std::chrono::steady_clock::time_point start_time;

        void setup_consumer_threads();
        void consumer_func();
        usize fetch_messages(std::vector<LogMsg>& target);

    public:
        explicit Logger(const LoggerConfig& cfg = LoggerConfig{}, memory_resource* mem = get_default_resource());
        ~Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        [[nodiscard]] memory_resource* get_allocator() const noexcept { return mem_res; }
        [[nodiscard]] const LoggerConfig& get_config() const noexcept { return config; }

        /**
         * @brief 注册并缓存模块头字符串 (防止外部 string_view 悬垂)
         */
        std::string_view register_header(std::string_view val);

        /**
         * @brief 刷新所有输出目标
         */
        void flush_targets();

        /**
         * @brief 强制处理并清空当前队列中所有剩余消息并刷新
         */
        void flush();

        /**
         * @brief 批量向启用的过滤器和输出目标写入消息
         */
        void write_messages(std::span<LogMsg> msgs, bool autoflush = true);

        /**
         * @brief 提交一条日志 (支持 PMR 字符串所有权零拷贝转移)
         */
        bool push_message_pmr(
            int level,
            std::string_view head,
            pmr::string&& body,
            const LogMsgConfig& cfg,
            pmr::vector<LogCustomTag>* tags = nullptr
        );

        /**
         * @brief 提交一条普通 string_view 日志
         */
        bool push_message(int level, std::string_view header, std::string_view body, const LogMsgConfig& cfg);

        /**
         * @brief 注册并挂载新的输出目标或过滤器
         */
        template<class T, class... Args>
        std::shared_ptr<T> append_mod(std::string_view name, Args&&... args) {
            static_assert(IsLogMod<T>, "T must derive from LogTarget or LogFilter!");
            auto ptr = std::make_shared<T>(std::forward<Args>(args)...);

            std::lock_guard<std::mutex> lock(mod_lock);
            pmr::string key(name, mem_res);

            if constexpr (IsLogTarget<T>) {
                auto it = search_targets.find(key);
                if (it == search_targets.end()) {
                    targets.push_back(ptr);
                    search_targets.emplace(std::move(key), alib6::ref(targets, targets.size() - 1));
                } else {
                    it->second.get() = ptr;
                }
            } else {
                auto it = search_filters.find(key);
                if (it == search_filters.end()) {
                    filters.push_back(ptr);
                    search_filters.emplace(std::move(key), alib6::ref(filters, filters.size() - 1));
                } else {
                    it->second.get() = ptr;
                }
            }
            return ptr;
        }

        /**
         * @brief 移除指定名称的输出目标或过滤器
         */
        template<class T = LogTarget>
        bool remove_mod(std::string_view name) {
            static_assert(IsLogMod<T>, "T must derive from LogTarget or LogFilter!");
            std::lock_guard<std::mutex> lock(mod_lock);
            pmr::string key(name, mem_res);

            if constexpr (IsLogTarget<T>) {
                auto it = search_targets.find(key);
                if (it == search_targets.end()) return false;
                usize cached_idx = it->second.get_index();
                search_targets.erase(it);
                for (auto& [k, v] : search_targets) {
                    if (v.get_index() > cached_idx) v.set_index(v.get_index() - 1);
                }
                targets.erase(targets.begin() + cached_idx);
                return true;
            } else {
                auto it = search_filters.find(key);
                if (it == search_filters.end()) return false;
                usize cached_idx = it->second.get_index();
                search_filters.erase(it);
                for (auto& [k, v] : search_filters) {
                    if (v.get_index() > cached_idx) v.set_index(v.get_index() - 1);
                }
                filters.erase(filters.begin() + cached_idx);
                return true;
            }
        }

        void clear_filter() {
            std::lock_guard<std::mutex> lock(mod_lock);
            search_filters.clear();
            filters.clear();
        }

        void clear_target() {
            std::lock_guard<std::mutex> lock(mod_lock);
            search_targets.clear();
            targets.clear();
        }

        void clear_mod() {
            clear_target();
            clear_filter();
        }

        template<IsLogMod T>
        [[nodiscard]] T* get_mod_raw(std::string_view name) {
            std::lock_guard<std::mutex> lock(mod_lock);
            pmr::string key(name, mem_res);
            if constexpr (IsLogTarget<T>) {
                auto it = search_targets.find(key);
                if (it == search_targets.end()) return nullptr;
                return dynamic_cast<T*>(it->second.get().get());
            } else {
                auto it = search_filters.find(key);
                if (it == search_filters.end()) return nullptr;
                return dynamic_cast<T*>(it->second.get().get());
            }
        }
    };

    /**
     * @brief 日志输出工厂 (为具体业务模块提供便捷的格式化与流式输出支持)
     */
    struct LogFactory {
        Logger& logger;
        LogFactoryConfig cfg;

        [[nodiscard]] memory_resource* get_allocator() const noexcept {
            return logger.get_allocator();
        }

        [[nodiscard]] const LogMsgConfig& get_msg_config() const noexcept {
            return cfg.msg;
        }

        explicit LogFactory(Logger& binded, const LogFactoryConfig& c = LogFactoryConfig{})
            : logger(binded), cfg(c) {
            if (!cfg.header.empty()) {
                cfg.header = binded.register_header(cfg.header);
            }
        }

        LogFactory(
            Logger& binded,
            std::string_view header,
            int def_level = static_cast<int>(LogLevel::Info),
            LogFactoryConfig::LevelKeepFn level_should_keep = nullptr,
            const LogMsgConfig& msg = LogMsgConfig{}
        ) : logger(binded)
          , cfg(header, def_level, level_should_keep, msg) {
            if (!cfg.header.empty()) {
                cfg.header = binded.register_header(cfg.header);
            }
        }

        bool log(int level, std::string_view message) {
            if (cfg.level_should_keep && !cfg.level_should_keep(level)) return false;
            return logger.push_message(level, cfg.header, message, cfg.msg);
        }

        bool log(LogLevel level, std::string_view message) {
            return log(static_cast<int>(level), message);
        }

        template<class... Args>
        bool log(int level, std::string_view fmt, Args&&... args) {
            if (cfg.level_should_keep && !cfg.level_should_keep(level)) return false;
            pmr::string str(logger.get_allocator());
            std::vformat_to(
                std::back_inserter(str),
                fmt,
                std::make_format_args(args...)
            );
            return logger.push_message_pmr(level, cfg.header, std::move(str), cfg.msg);
        }

        template<class... Args>
        bool log(LogLevel level, std::string_view fmt, Args&&... args) {
            return log(static_cast<int>(level), fmt, std::forward<Args>(args)...);
        }

        bool log_pmr(
            int level,
            pmr::string&& pmr_data,
            const LogMsgConfig& mcfg,
            pmr::vector<LogCustomTag>& tags
        ) {
            if (cfg.level_should_keep && !cfg.level_should_keep(level)) return false;
            return logger.push_message_pmr(level, cfg.header, std::move(pmr_data), mcfg, tags.empty() ? nullptr : &tags);
        }

        // ==================== 流式操作符入口 ====================

        [[nodiscard]] StreamedContext<LogFactory> operator()() {
            bool valid = !cfg.level_should_keep || cfg.level_should_keep(cfg.def_level);
            return StreamedContext<LogFactory>(cfg.def_level, *this, valid);
        }

        [[nodiscard]] StreamedContext<LogFactory> operator()(int spec_level) {
            bool valid = !cfg.level_should_keep || cfg.level_should_keep(spec_level);
            return StreamedContext<LogFactory>(spec_level, *this, valid);
        }

        [[nodiscard]] StreamedContext<LogFactory> operator()(LogLevel spec_level) {
            return operator()(static_cast<int>(spec_level));
        }

        template<class T>
        [[nodiscard]] StreamedContext<LogFactory> operator<<(T&& t) {
            bool valid = !cfg.level_should_keep || cfg.level_should_keep(cfg.def_level);
            return StreamedContext<LogFactory>(cfg.def_level, *this, valid) << std::forward<T>(t);
        }
    };

} // namespace alib6::log
