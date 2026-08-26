/**
 * @file io.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 文件读写、目录遍历与文件系统树形条目结构 (接口定义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cstdio>
#include <filesystem>

export module alib6.core:io;
import std;
import :types;
import :memory;
import :concepts;
import :error;

namespace pmr = std::pmr;

export namespace alib6::io {

#ifdef _WIN32
    constexpr char path_sep = '\\';
#else
    constexpr char path_sep = '/';
#endif

    /**
     * @brief 写入连续数据到指定路径文件
     */
    template<class T>
    usize write_all(std::string_view path, const T& input, ErrorWrapper err = {}) {
        if (path.empty()) {
            err.report("write_all failed: path is empty");
            return std::numeric_limits<usize>::max();
        }

        std::string null_terminated_path(path);
        FILE* f = std::fopen(null_terminated_path.c_str(), "wb");
        if (!f) {
            err.report("write_all: failed to open file '{}' for writing", path);
            return std::numeric_limits<usize>::max();
        }

        usize written = 0;
        if constexpr (std::convertible_to<const T&, std::string_view>) {
            std::string_view sv(input);
            written = std::fwrite(sv.data(), sizeof(char), sv.size(), f);
        } else if constexpr (requires { input.data(); input.size(); }) {
            written = std::fwrite(input.data(), sizeof(typename T::value_type), input.size(), f);
        }

        std::fclose(f);
        return written;
    }

    struct FileEntry;

    /**
     * @brief 加载指定路径的 FileEntry 节点信息
     */
    [[nodiscard]] FileEntry load_entry(
        std::string_view path,
        bool force_existence = true,
        memory_resource* mem = get_default_resource(),
        ErrorWrapper err = {}
    );

    /**
     * @brief 文件与目录条目抽象结构（支持树形嵌套缓存扫描）
     */
    struct FileEntry {
        enum class Type : i8 {
            none = 0,      not_found = -1, 
            regular = 1,   directory = 2, 
            symlink = 3,   block = 4,
            character = 5, fifo = 6,
            socket = 7,    unknown = 8
        };

        pmr::string path;                                      ///< 文件绝对路径
        Type type{Type::not_found};                           ///< 文件类型
        std::filesystem::file_time_type last_write{};         ///< 最后修改时间

    private:
        mutable pmr::unordered_map<pmr::string, FileEntry> subs;      ///< 子目录/子文件缓存
        mutable pmr::unordered_map<pmr::string, FileEntry> flat_subs; ///< 扁平查询缓存
        mutable bool scanned{false};

        static const FileEntry& gen_invalid();

    public:
        explicit FileEntry(memory_resource* mem = get_default_resource())
            : path(mem), subs(mem), flat_subs(mem) {}

        FileEntry(pmr::string p, Type t, memory_resource* mem = get_default_resource())
            : path(std::move(p)), type(t), subs(mem), flat_subs(mem) {}

        FileEntry(const FileEntry& other)
            : path(other.path), type(other.type), last_write(other.last_write)
            , subs(other.subs), flat_subs(other.flat_subs), scanned(other.scanned) {}

        FileEntry(FileEntry&& other) noexcept
            : path(std::move(other.path)), type(other.type), last_write(other.last_write)
            , subs(std::move(other.subs)), flat_subs(std::move(other.flat_subs)), scanned(other.scanned) {}

        FileEntry& operator=(const FileEntry& other) {
            if (this != &other) {
                path = other.path;
                type = other.type;
                last_write = other.last_write;
                subs = other.subs;
                flat_subs = other.flat_subs;
                scanned = other.scanned;
            }
            return *this;
        }

        FileEntry& operator=(FileEntry&& other) noexcept {
            if (this != &other) {
                path = std::move(other.path);
                type = other.type;
                last_write = other.last_write;
                subs = std::move(other.subs);
                flat_subs = std::move(other.flat_subs);
                scanned = other.scanned;
            }
            return *this;
        }

        [[nodiscard]] bool invalid() const noexcept {
            return (type == Type::not_found) || path.empty();
        }

        [[nodiscard]] bool is_directory() const noexcept {
            return type == Type::directory;
        }

        [[nodiscard]] usize file_size(ErrorWrapper err = {}) const;

        void rescan() const {
            scanned = false;
            scan_subs();
        }

        void scan_subs() const;

        [[nodiscard]] usize elements_size() const {
            scan_subs();
            return subs.size();
        }

        auto begin() const { scan_subs(); return subs.cbegin(); }
        auto end() const { scan_subs(); return subs.cend(); }
        auto begin() { scan_subs(); return subs.begin(); }
        auto end() { scan_subs(); return subs.end(); }

        const FileEntry& operator[](std::string_view str) const;
        const FileEntry& operator()(std::string_view str) const;

        template<CanExtendString T> 
        usize read(T& output, usize read_count = 0, ErrorWrapper err = {}) const {
            if (read_count == 0) {
                read_count = file_size(err);
                if (read_count == std::numeric_limits<usize>::max()) return read_count;
            }

            FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) {
                err.report("read: failed to open file '{}'", path);
                return std::numeric_limits<usize>::max();
            }

            usize old_size = output.size();
            output.resize(old_size + read_count);
            usize actual_read = std::fread(output.data() + old_size, sizeof(char), read_count, f);
            std::fclose(f);
            return actual_read;
        }

        [[nodiscard]] pmr::string read(
            usize read_count = 0,
            memory_resource* mem = get_default_resource(),
            ErrorWrapper err = {}
        ) const {
            pmr::string str(mem);
            read(str, read_count, err);
            return str;
        }

        template<class T>
        usize write_all(const T& input, ErrorWrapper err = {}) const {
            return alib6::io::write_all(path, input, err);
        }

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "FileEntry(path=\"{}\", type={})", path, static_cast<int>(type));
        }
    };

    /**
     * @brief 读取整个文件到目标缓冲
     */
    template<CanExtendString T>
    inline usize read_all(std::string_view path, T& output, ErrorWrapper err = {}) {
        FileEntry entry = load_entry(path, true, get_default_resource(), err);
        if (entry.invalid()) return std::numeric_limits<usize>::max();
        return entry.read(output, 0, err);
    }

    /**
     * @brief 简易读取文件内容返回 PMR string
     */
    inline pmr::string read_all(
        std::string_view path,
        usize read_count = 0,
        memory_resource* mem = get_default_resource(),
        ErrorWrapper err = {}
    ) {
        FileEntry entry = load_entry(path, true, mem, err);
        if (entry.invalid()) return pmr::string(mem);
        return entry.read(read_count, mem, err);
    }

    /**
     * @brief 目录遍历结果集 (PMR)
     */
    struct TraverseData {
        pmr::string root_absolute;
        pmr::vector<FileEntry> targets_absolute;

        explicit TraverseData(memory_resource* mem = get_default_resource())
            : root_absolute(mem), targets_absolute(mem) {}

        [[nodiscard]] std::string_view try_get_relative(const FileEntry& entry) const noexcept;

        auto begin() noexcept { return targets_absolute.begin(); }
        auto end() noexcept { return targets_absolute.end(); }
        auto begin() const noexcept { return targets_absolute.cbegin(); }
        auto end() const noexcept { return targets_absolute.cend(); }

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(
                std::back_inserter(target),
                "TraverseData(root=\"{}\", count={})",
                root_absolute, targets_absolute.size()
            );
        }
    };

    /**
     * @brief 目录遍历配置项
     */
    struct TraverseConfig {
        int depth{-1};                                                   ///< 递归深度，-1 表示无限深度
        std::vector<std::function<bool(std::string_view)>> ignore;      ///< 相对路径过滤闭包规则
        std::vector<FileEntry::Type> keep{FileEntry::Type::regular};     ///< 保留的文件类型
        bool allow[16]{false};

        void build() noexcept;
    };

    /**
     * @brief 遍历指定目录下的所有文件并返回结构化数据
     */
    [[nodiscard]] TraverseData traverse_files(
        std::string_view file_path,
        TraverseConfig cfg = {},
        memory_resource* mem = get_default_resource(),
        ErrorWrapper err = {}
    );

} // namespace alib6::io
