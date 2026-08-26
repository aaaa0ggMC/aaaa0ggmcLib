/**
 * @file io.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 文件条目加载与目录树形遍历实现
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
#include <deque>

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6::io {

    const FileEntry& FileEntry::gen_invalid() {
        static const FileEntry invalid_node;
        return invalid_node;
    }

    usize FileEntry::file_size(ErrorWrapper err) const {
        std::error_code ec;
        auto sz = std::filesystem::file_size(path.c_str(), ec);
        if (ec) {
            err.report("file_size: failed for '{}': {}", path, ec.message());
            return std::numeric_limits<usize>::max();
        }
        return sz;
    }

    void TraverseConfig::build() noexcept {
        for (auto val : keep) {
            int idx = static_cast<int>(val) + 1;
            if (idx >= 0 && idx < 16) {
                allow[idx] = true;
            }
        }
    }

    std::string_view TraverseData::try_get_relative(const FileEntry& entry) const noexcept {
        if (entry.path.size() >= root_absolute.size()) {
            return std::string_view(entry.path).substr(root_absolute.size());
        }
        return entry.path;
    }

    TraverseData traverse_files(
        std::string_view file_path,
        TraverseConfig cfg,
        memory_resource* mem,
        ErrorWrapper err
    ) {
        TraverseData ret(mem);
        std::error_code ec;
        auto root = std::filesystem::path(file_path);

        if (root.is_relative()) {
            root = std::filesystem::current_path(ec) / root;
        }
        ret.root_absolute = pmr::string(root.string(), mem);

        if (ret.root_absolute.empty()) {
            err.report("traverse_files: cannot locate root directory for '{}'", file_path);
            return ret;
        }

        if (ret.root_absolute.back() != '/' && ret.root_absolute.back() != '\\') {
            ret.root_absolute.push_back(path_sep);
        }

        cfg.build();

        if (!std::filesystem::is_directory(root, ec) || ec) {
            err.report("traverse_files: '{}' is not a valid directory ({})", file_path, ec.message());
            return ret;
        }

        int current_depth = 0;
        std::deque<std::filesystem::path> current_layer;
        std::deque<std::filesystem::path> next_layer;
        current_layer.emplace_back(root);

        while (!current_layer.empty() && (current_depth < cfg.depth || cfg.depth < 0)) {
            for (const auto& p : current_layer) {
                std::filesystem::directory_iterator direc(p, std::filesystem::directory_options::skip_permission_denied, ec);
                if (ec) {
                    err.report("traverse_files: permission denied or error accessing '{}'", p.string());
                    continue;
                }

                for (const auto& sub : direc) {
                    FileEntry entry(mem);
                    entry.type = static_cast<FileEntry::Type>(sub.status(ec).type());
                    entry.path = pmr::string(sub.path().string(), mem);
                    entry.last_write = sub.last_write_time(ec);

                    if (entry.path.empty()) continue;

                    bool keep_entry = true;
                    if (sub.is_directory(ec)) {
                        if (entry.path.back() != '/' && entry.path.back() != '\\') {
                            entry.path.push_back(path_sep);
                        }
                        if (current_depth + 1 != cfg.depth) {
                            next_layer.push_back(sub.path());
                        }
                    }

                    int type_idx = static_cast<int>(entry.type) + 1;
                    if (type_idx >= 0 && type_idx < 16 && cfg.allow[type_idx]) {
                        std::string_view rel = ret.try_get_relative(entry);
                        for (const auto& fn : cfg.ignore) {
                            if (fn && fn(rel)) {
                                keep_entry = false;
                                break;
                            }
                        }
                    } else {
                        keep_entry = false;
                    }

                    if (keep_entry) {
                        ret.targets_absolute.emplace_back(std::move(entry));
                    }
                }
            }

            ++current_depth;
            current_layer = std::move(next_layer);
            next_layer.clear();
        }

        return ret;
    }

    FileEntry load_entry(
        std::string_view path,
        bool force_existence,
        memory_resource* mem,
        ErrorWrapper err
    ) {
        FileEntry entry(mem);
        std::error_code ec;
        auto s = std::filesystem::status(path, ec);

        entry.path = pmr::string(path, mem);
        if (ec) {
            if (force_existence) {
                err.report("load_entry: file '{}' does not exist or inaccessible ({})", path, ec.message());
            }
            entry.type = FileEntry::Type::not_found;
            return entry;
        }

        if (std::filesystem::is_directory(path, ec)) {
            char ch = entry.path.empty() ? '\0' : entry.path.back();
            if (ch != '\\' && ch != '/') {
                entry.path.push_back(path_sep);
            }
        }

        entry.last_write = std::filesystem::last_write_time(path, ec);
        entry.type = static_cast<FileEntry::Type>(s.type());
        return entry;
    }

    void FileEntry::scan_subs() const {
        if (scanned || type != Type::directory) return;
        scanned = true;
        subs.clear();

        TraverseConfig cfg;
        cfg.depth = 1;
        cfg.keep = {
            Type::regular, Type::directory, 
            Type::symlink, Type::block,
            Type::character, Type::fifo,
            Type::socket, Type::unknown
        };

        auto data = traverse_files(path, cfg, subs.get_allocator().resource());
        for (auto& entry : data.targets_absolute) {
            pmr::string key(data.try_get_relative(entry), subs.get_allocator());
            subs.emplace(std::move(key), std::move(entry));
        }
    }

    const FileEntry& FileEntry::operator()(std::string_view str) const {
        if (scanned) return (*this)[str];
        std::filesystem::path rpath(path.c_str());
        rpath /= str;
        FileEntry entry = load_entry(rpath.string(), false, flat_subs.get_allocator().resource());
        if (entry.invalid()) return gen_invalid();
        auto v = flat_subs.emplace(entry.path, std::move(entry));
        return v.first->second;
    }

    const FileEntry& FileEntry::operator[](std::string_view data) const {
        if (data.empty() || type != Type::directory) return gen_invalid();
        if (!scanned) scan_subs();

        pmr::string key(data, subs.get_allocator());
        auto it = subs.find(key);
        if (it == subs.end()) {
            char ch = key.empty() ? '\0' : key.back();
            if (ch != '/' && ch != '\\') {
                key.push_back('/');
                it = subs.find(key);
                if (it == subs.end()) {
                    key.back() = '\\';
                    it = subs.find(key);
                    return (it == subs.end()) ? gen_invalid() : it->second;
                }
                return it->second;
            }
            return gen_invalid();
        }
        return it->second;
    }

} // namespace alib6::io
