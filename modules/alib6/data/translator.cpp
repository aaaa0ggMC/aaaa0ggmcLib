/**
 * @file translator.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief i18n 国际化翻译系统实现文件
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <string_view>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <format>

module alib6.data;

import alib6.core;

namespace pmr = std::pmr;

namespace alib6::data {

    // ========================================================================
    // FlattenTranslator Implementation
    // ========================================================================

    void FlattenTranslator::build_mapper() {
        mapper.clear();
        mapper.reserve(offsets.size());
        for (const auto& d : offsets) {
            mapper.emplace(
                std::string_view(key_buffer.data() + d.key.first, d.key.second - d.key.first),
                std::string_view(value_buffer.data() + d.value.first, d.value.second - d.value.first)
            );
        }
    }

    FlattenTranslator::FlattenTranslator(const FlattenTranslator& v)
        : GenericTranslator(v)
        , type(v.type)
        , value_buffer(v.value_buffer)
        , key_buffer(v.key_buffer)
        , mapper(v.mapper.get_allocator())
        , offsets(v.offsets) {
        copy_allocator = v.copy_allocator;
        build_mapper();
    }

    FlattenTranslator::FlattenTranslator(FlattenTranslator&& v) noexcept
        : GenericTranslator(std::move(v))
        , type(v.type)
        , value_buffer(std::move(v.value_buffer))
        , key_buffer(std::move(v.key_buffer))
        , mapper(std::move(v.mapper))
        , offsets(std::move(v.offsets)) {
        copy_allocator = v.copy_allocator;
    }

    FlattenTranslator& FlattenTranslator::operator=(const FlattenTranslator& v) {
        if (this != &v) {
            GenericTranslator::operator=(v);
            type = v.type;
            value_buffer = v.value_buffer;
            key_buffer = v.key_buffer;
            offsets = v.offsets;
            copy_allocator = v.copy_allocator;
            build_mapper();
        }
        return *this;
    }

    FlattenTranslator& FlattenTranslator::operator=(FlattenTranslator&& v) noexcept {
        if (this != &v) {
            GenericTranslator::operator=(std::move(v));
            type = v.type;
            value_buffer = std::move(v.value_buffer);
            key_buffer = std::move(v.key_buffer);
            mapper = std::move(v.mapper);
            offsets = std::move(v.offsets);
            copy_allocator = v.copy_allocator;
        }
        return *this;
    }

    std::string_view FlattenTranslator::get_key_value(std::string_view key) const {
        auto it = mapper.find(key);
        if (it == mapper.end()) return key;
        return it->second;
    }

    // ========================================================================
    // Translator Implementation
    // ========================================================================

    bool Translator::default_is_trfile(std::string_view file) {
        return file.ends_with(".json") || file.ends_with(".toml");
    }

    Translator::Translator(pmr::memory_resource* mem)
        : translations(mem)
        , res(mem)
        , schema(mem)
        , current_language(mem) {
        copy_allocator = mem;

        schema["id"] = "REQUIRED TYPE STRING VALIDATE not_empty";
        schema["title"] = "REQUIRED TYPE STRING VALIDATE not_empty";

        validator.emplace_validate_method("not_empty", [](AData& node, pmr::vector<pmr::string>&) {
            if (node.is_value()) return !node.to<std::string_view>().empty();
            return false;
        });

        validator.from_adata(schema);
    }

    bool Translator::load_from_memory(std::string_view data, ErrorWrapper err) {
        AData tmp(res);
        data::JSON parser;
        if (!parser.parse(data, tmp)) {
            err.report("Failed to parse translation JSON", CodeWithLocation(400));
            return false;
        }

        vali_result.reset();
        if (!validator.validate(tmp, vali_result)) {
            err.report("Validator failed on translation file content", CodeWithLocation(400));
            return false;
        }

        auto id_view = tmp["id"].to<std::string_view>();
        if (current_language.empty()) {
            current_language = id_view;
        }

        translations[id_view].merge(std::move(tmp));
        return true;
    }

    Translator::Result Translator::load_from_entry(
        std::string_view path,
        IsTRFileFn is_trfile,
        ErrorWrapper err
    ) {
        auto entry = io::load_entry(path, true, res, err);
        return load_from_entry(entry, is_trfile, err);
    }

    Translator::Result Translator::load_from_entry(
        const io::FileEntry& entry,
        IsTRFileFn is_trfile,
        ErrorWrapper err
    ) {
        Result r;
        if (entry.invalid()) {
            r.ecode = -1;
            err.report("Invalid entry path for translations", CodeWithLocation(404));
            return r;
        }

        pmr::string buf(res);

        if (entry.is_directory()) {
            io::TraverseConfig cfg;
            cfg.depth = 1;
            cfg.keep = { io::FileEntry::Type::regular, io::FileEntry::Type::symlink };
            auto data = io::traverse_files(entry.path, cfg, res, err);

            std::vector<io::FileEntry> files(data.targets_absolute.begin(), data.targets_absolute.end());
            std::sort(files.begin(), files.end(), [](const io::FileEntry& a, const io::FileEntry& b) {
                return a.path < b.path;
            });

            for (const auto& file : files) {
                if (is_trfile(file.path)) {
                    buf.clear();
                    file.read(buf, 0, err);
                    if (!buf.empty() && load_from_memory(buf, err)) {
                        if (r.enable_success) r.success_files.emplace_back(file.path);
                        r.success_count += 1;
                    } else {
                        if (r.enable_failures) r.failure_files.emplace_back(file.path);
                        r.failure_count += 1;
                    }
                }
            }
        } else if (is_trfile(entry.path)) {
            buf.clear();
            entry.read(buf, 0, err);
            if (!buf.empty() && load_from_memory(buf, err)) {
                if (r.enable_success) r.success_files.emplace_back(entry.path);
                r.success_count += 1;
            } else {
                if (r.enable_failures) r.failure_files.emplace_back(entry.path);
                r.failure_count += 1;
            }
        }

        if (r.success_count == 0) {
            r.ecode = -1;
        }
        return r;
    }

    bool Translator::switch_language(std::string_view lan) {
        if (!translations.is_object()) return false;
        if (!translations.object().contains(lan)) return false;
        current_language = lan;
        return true;
    }

    std::string_view Translator::get_key_value_dots(std::string_view key) const {
        if (!translations.is_object() || current_language.empty()) return key;
        auto it = translations.object().find(current_language);
        if (it == translations.object().end()) return key;

        const AData* current = &it.second();
        auto vals = alib6::str::split(key, '.');

        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (current->is_array()) {
                std::from_chars_result r{};
                int idx = alib6::ext::to_T<int>(vals[i], &r);
                if (r.ec == std::errc()) {
                    current = current->array().at_ptr(idx);
                    if (!current) return key;
                } else {
                    return key;
                }
            } else if (current->is_object()) {
                std::string_view path = vals[i];
                const auto& obj = current->object();
                while (true) {
                    auto itt = obj.find(path);
                    if (itt == obj.end()) {
                        ++i;
                        if (i < vals.size()) {
                            path = std::string_view(path.data(), vals[i].data() + vals[i].length() - path.data());
                            continue;
                        } else {
                            return key;
                        }
                    }
                    current = &itt.second();
                    break;
                }
            } else {
                return key;
            }
        }

        if (current->is_value()) return current->to<std::string_view>();
        return key;
    }

    std::string_view Translator::get_key_value(std::string_view key) const {
        if (!translations.is_object() || current_language.empty()) return key;
        auto it = translations.object().find(current_language);
        if (it == translations.object().end()) return key;

        const AData& current = it.second();
        auto ptr = current.jump_ptr(key, false);
        if (ptr && ptr->is_value()) {
            return ptr->value().to<std::string_view>();
        }
        return key;
    }

    // ========================================================================
    // Unified Flattening Implementation
    // ========================================================================

    std::optional<FlattenTranslator> Translator::flatten_impl(
        const AData& translations,
        std::string_view target_lang,
        std::string_view current_language,
        FlattenTranslator::Type mode,
        std::size_t reserve_size,
        pmr::memory_resource* alloc_mem
    ) {
        std::string_view key = target_lang.empty() ? current_language : target_lang;
        if (key.empty() || !translations.is_object()) return std::nullopt;

        auto it = translations.object().find(key);
        if (it == translations.object().end()) return std::nullopt;

        FlattenTranslator flat(alloc_mem);
        flat.type = mode;
        flat.key_buffer.reserve(reserve_size);
        flat.value_buffer.reserve(reserve_size);

        struct Frame {
            const AData* node{nullptr};
            int depth{0};
            int index{-1};
            std::string_view segment{""};
        };

        std::vector<Frame> frames;
        std::vector<std::size_t> seps;
        pmr::string loc(alloc_mem);

        frames.emplace_back(&it.second(), 0, -1, "");

        auto pop_segment = [&] {
            if (!seps.empty()) {
                loc.resize(seps.back());
                seps.pop_back();
            }
        };

        while (!frames.empty()) {
            auto d = frames.back();
            frames.pop_back();

            if (d.node == nullptr) {
                pop_segment();
                continue;
            }

            auto push_segment = [&] {
                if (d.index < 0 && d.segment.empty()) return;
                seps.push_back(loc.size());

                if (mode == FlattenTranslator::Type::Dots) {
                    if (d.depth > 1) loc.push_back('.');
                    if (d.index >= 0) {
                        loc += alib6::ext::to_string(d.index);
                    } else {
                        loc += d.segment;
                    }
                } else { // JsonP
                    loc.push_back('/');
                    if (d.index >= 0) {
                        loc += alib6::ext::to_string(d.index);
                    } else {
                        for (char ch : d.segment) {
                            if (ch == '~') loc.append("~0");
                            else if (ch == '/') loc.append("~1");
                            else loc.push_back(ch);
                        }
                    }
                }
            };

            if (d.node->is_array()) {
                push_segment();
                const auto& arr = d.node->array();
                frames.emplace_back(nullptr);
                for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(arr.size()) - 1; i >= 0; --i) {
                    frames.emplace_back(&arr[i], d.depth + 1, static_cast<int>(i), "");
                }
            } else if (d.node->is_object()) {
                push_segment();
                const auto& obj = d.node->object();
                frames.emplace_back(nullptr);
                // 收集节点倒序压栈，保证遍历正序
                std::vector<std::pair<std::string_view, const AData*>> entries;
                entries.reserve(obj.size());
                for (auto p : obj) {
                    entries.emplace_back(p.first(), &p.second());
                }
                for (auto it_e = entries.rbegin(); it_e != entries.rend(); ++it_e) {
                    frames.emplace_back(it_e->second, d.depth + 1, -1, it_e->first);
                }
            } else if (d.node->is_null()) {
                continue;
            } else {
                push_segment();
                std::size_t kb = flat.key_buffer.size();
                flat.key_buffer += loc;
                std::size_t vb = flat.value_buffer.size();
                flat.value_buffer += d.node->to<std::string_view>();

                flat.offsets.emplace_back(
                    std::pair{kb, flat.key_buffer.size()},
                    std::pair{vb, flat.value_buffer.size()}
                );
                pop_segment();
            }
        }

        flat.build_mapper();
        return flat;
    }

    std::optional<FlattenTranslator> Translator::flatten_dots(
        std::string_view key,
        std::size_t reserve,
        pmr::memory_resource* mem
    ) const {
        return flatten_impl(translations, key, current_language, FlattenTranslator::Type::Dots, reserve, mem ? mem : res);
    }

    std::optional<FlattenTranslator> Translator::flatten_jsonp(
        std::string_view key,
        std::size_t reserve,
        pmr::memory_resource* mem
    ) const {
        return flatten_impl(translations, key, current_language, FlattenTranslator::Type::JsonP, reserve, mem ? mem : res);
    }

} // namespace alib6::data
