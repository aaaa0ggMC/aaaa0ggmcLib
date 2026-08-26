/**
 * @file json.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief RapidJSON SAX 解析器与格式化转储实现 (alib6.data)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <cmath>

module alib6.data;
import std;
import alib6.core;

namespace pmr = std::pmr;

namespace alib6::data {

    struct ADataHandler {
        pmr::vector<AData*> stack;
        pmr::string last_key;

        explicit ADataHandler(AData& root)
            : stack(root.get_allocator()), last_key(root.get_allocator()) {
            stack.push_back(&root);
        }

        [[nodiscard]] AData& current() noexcept { return *stack.back(); }

        AData& prepare_node() {
            AData& c = current();
            if (c.get_type() == AData::TObject) {
                return c[last_key];
            } else if (c.get_type() == AData::TArray) {
                return c.array().values.emplace_back(c.get_allocator());
            }
            return c;
        }

        bool Null() { prepare_node().set_null(); return true; }
        bool Bool(bool b) { prepare_node() = b; return true; }
        bool Int(int i) { prepare_node() = static_cast<i64>(i); return true; }
        bool Uint(unsigned u) { prepare_node() = static_cast<i64>(u); return true; }
        bool Int64(int64_t i) { prepare_node() = static_cast<i64>(i); return true; }
        bool Uint64(uint64_t u) { prepare_node() = static_cast<i64>(u); return true; }
        bool Double(double d) { prepare_node() = d; return true; }

        bool String(const char* str, rapidjson::SizeType length, bool) {
            prepare_node() = std::string_view(str, length);
            return true;
        }

        bool StartObject() {
            AData& node = prepare_node();
            node.set<AData::Object>();
            stack.push_back(&node);
            return true;
        }

        bool Key(const char* str, rapidjson::SizeType length, bool) {
            last_key = std::string_view(str, length);
            return true;
        }

        bool EndObject(rapidjson::SizeType) {
            stack.pop_back();
            return true;
        }

        bool StartArray() {
            AData& node = prepare_node();
            node.set<AData::Array>();
            stack.push_back(&node);
            return true;
        }

        bool EndArray(rapidjson::SizeType) {
            stack.pop_back();
            return true;
        }

        bool RawNumber(const char*, rapidjson::SizeType, bool) {
            return false;
        }
    };

    bool JSON::parse(std::string_view v, AData& root) {
        root.set_null();
        using namespace rapidjson;

        char local_buffer[16 * 1024];
        MemoryPoolAllocator<> stack_alloc(local_buffer, sizeof(local_buffer));

        GenericReader<UTF8<>, UTF8<>, MemoryPoolAllocator<>> reader(&stack_alloc);
        MemoryStream ss(v.data(), v.size());
        ADataHandler handler(root);

        ParseResult res;
        if (cfg.rapidjson_recursive) {
            if (cfg.allow_comments) {
                res = reader.Parse<kParseDefaultFlags | kParseCommentsFlag>(ss, handler);
            } else {
                res = reader.Parse<kParseDefaultFlags>(ss, handler);
            }
        } else {
            if (cfg.allow_comments) {
                res = reader.Parse<kParseIterativeFlag | kParseCommentsFlag>(ss, handler);
            } else {
                res = reader.Parse<kParseIterativeFlag>(ss, handler);
            }
        }
        return static_cast<bool>(res);
    }

    JSON::DumpResult JSON::__internal_dump(__dump_fn fn, void* p, const AData& root) const {
        struct Frame {
            const AData* data;
            int depth;
            std::optional<std::string_view> name;
            int index;
            bool last_child;
            enum Action {
                PROCESS,
                WRITE_CLOSE_OBJ,
                WRITE_CLOSE_ARR
            };
            Action action{PROCESS};
        };

        pmr::vector<Frame> queue(root.get_allocator());
        pmr::string indents(root.get_allocator());
        DumpResult rt = Success;

        std::string_view place_comma = cfg.compact_lines ? "," : ",\n";
        std::string_view place_next = cfg.compact_lines ? (cfg.compact_spaces ? "" : " ") : "\n";
        std::string_view arr_new = "";
        std::string_view obj_new = "";

        if (cfg.compact_lines) {
            if (cfg.compact_spaces) {
                arr_new = "[";
                obj_new = "{";
            } else {
                arr_new = "[ ";
                obj_new = "{ ";
            }
        } else {
            arr_new = "[\n";
            obj_new = "{\n";
        }

        auto get_indent = [this, &indents](int indent) -> std::string_view {
            if (indent < 0) indent = 0;
            if (indents.size() < static_cast<usize>(indent)) {
                indents.resize(static_cast<usize>(indent), cfg.dump_indent_char);
            }
            return std::string_view(indents.data(), static_cast<usize>(indent));
        };

        queue.push_back({
            &root,
            0,
            std::nullopt,
            -1,
            true,
            Frame::PROCESS
        });

        auto place = [&](Frame& current) {
            if (queue.empty()) return;
            if (!current.last_child) {
                fn(place_comma, p);
            } else {
                fn(place_next, p);
            }
        };

        struct ObjFrame {
            std::string_view name;
            const AData* node;
        };
        pmr::vector<ObjFrame> frames(root.get_allocator());

        while (!queue.empty()) {
            Frame current = queue.back();
            queue.pop_back();

            if (current.action == Frame::WRITE_CLOSE_OBJ) {
                if (!cfg.compact_lines) fn(get_indent(current.depth * static_cast<int>(cfg.dump_indent)), p);
                fn("}", p);
                place(current);
                continue;
            } else if (current.action == Frame::WRITE_CLOSE_ARR) {
                if (!cfg.compact_lines) fn(get_indent(current.depth * static_cast<int>(cfg.dump_indent)), p);
                fn("]", p);
                place(current);
                continue;
            }

            if (!cfg.compact_lines) fn(get_indent(current.depth * static_cast<int>(cfg.dump_indent)), p);

            if (current.name) {
                fn("\"", p);
                fn(*current.name, p);
                if (cfg.compact_spaces) fn("\":", p);
                else fn("\" : ", p);
            }

            if (current.data->is_null()) {
                fn("null", p);
                place(current);
            } else if (current.data->is_value()) {
                const auto& v = current.data->value();
                if (v.get_type() != Value::STRING) {
                    if (cfg.warn_when_nan && v.get_type() == Value::FLOATING) {
                        double val = v.to<double>();
                        if (std::isnan(val)) {
                            rt = EncounteredNAN;
                        } else if (std::isinf(val)) {
                            rt = EncounteredINF;
                        }
                    }

                    if (cfg.float_precision >= 0 && v.get_type() == Value::FLOATING) {
                        std::string double_str = std::format("{:.{}f}", v.to<double>(), cfg.float_precision);
                        fn(double_str, p);
                    } else {
                        fn(v.to<std::string>(), p);
                    }
                } else {
                    fn("\"", p);
                    auto escaped = alib6::str::escape(v.to<std::string_view>(), cfg.ensure_ascii);
                    fn(escaped, p);
                    fn("\"", p);
                }
                place(current);
            } else if (current.data->is_array()) {
                usize sz = current.data->array().size();
                fn(arr_new, p);
                queue.push_back({
                    nullptr,
                    current.depth,
                    std::nullopt,
                    -1,
                    current.last_child,
                    Frame::WRITE_CLOSE_ARR
                });

                const auto& arr = current.data->array();
                for (usize i = arr.size(); i > 0; --i) {
                    queue.push_back({
                        &arr[static_cast<ptrdiff_t>(i - 1)],
                        current.depth + 1,
                        std::nullopt,
                        static_cast<int>(i - 1),
                        (i == sz),
                        Frame::PROCESS
                    });
                }
            } else if (current.data->is_object()) {
                usize sz = current.data->object().size();
                fn(obj_new, p);
                queue.push_back({
                    nullptr,
                    current.depth,
                    std::nullopt,
                    -1,
                    current.last_child,
                    Frame::WRITE_CLOSE_OBJ
                });

                if (cfg.sort_object) {
                    frames.clear();
                    for (auto proxy : current.data->object()) {
                        if (cfg.filter && cfg.filter(proxy.first(), proxy.second()) == JSONConfig::FilterOp::Discard) {
                            continue;
                        }
                        frames.push_back(ObjFrame{proxy.first(), &proxy.second()});
                    }

                    std::sort(frames.begin(), frames.end(), [this](const ObjFrame& a, const ObjFrame& b) {
                        return !cfg.sort_object(a.name, b.name);
                    });

                    usize index = frames.size();
                    for (const auto& proxy : frames) {
                        queue.push_back({
                            proxy.node,
                            current.depth + 1,
                            proxy.name,
                            -1,
                            (index == frames.size()),
                            Frame::PROCESS
                        });
                        --index;
                    }
                } else {
                    usize index = sz;
                    for (auto proxy : current.data->object()) {
                        if (cfg.filter && cfg.filter(proxy.first(), proxy.second()) == JSONConfig::FilterOp::Discard) {
                            continue;
                        }
                        queue.push_back({
                            &proxy.second(),
                            current.depth + 1,
                            proxy.first(),
                            -1,
                            (index == sz),
                            Frame::PROCESS
                        });
                        --index;
                    }
                }
            }
        }
        return rt;
    }

} // namespace alib6::data
