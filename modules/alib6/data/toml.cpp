/**
 * @file toml.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief toml++ 解析与格式化转储实现 (alib6.data)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <toml++/toml.hpp>
#include <sstream>
#include <cstddef>

module alib6.data;
import std;
import alib6.core;

namespace pmr = std::pmr;

namespace alib6::data {

    bool TOML::parse(std::string_view data, AData& node) {
        node.set_null();
        try {
            const toml::table table = toml::parse(data);
            struct Frame {
                AData* node;
                const toml::node* table;
            };
            struct ObjFrame {
                usize index;
                const toml::node* node;
            };
            std::vector<Frame> frames;
            std::vector<ObjFrame> obj_frames;
            frames.push_back({&node, &table});

            while (!frames.empty()) {
                Frame f = frames.back();
                frames.pop_back();

                f.table->visit([&](auto&& el) {
                    using T = std::decay_t<decltype(el)>;
                    if constexpr (toml::is_string<T> || toml::is_integer<T> || toml::is_floating_point<T> || toml::is_boolean<T>) {
                        f.node->rewrite(*el);
                    } else if constexpr (toml::is_date<T> || toml::is_time<T> || toml::is_date_time<T>) {
                        std::stringstream ss;
                        ss << el;
                        f.node->rewrite(ss.str());
                    } else if constexpr (toml::is_table<T>) {
                        const toml::table& tb = *f.table->as_table();
                        f.node->set<AData::Object>();
                        auto& obj = f.node->object();
                        obj_frames.clear();
                        for (auto& c : tb) {
                            obj_frames.push_back({obj.ensure_node(c.first).second, &c.second});
                        }
                        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(obj_frames.size()) - 1; i >= 0; --i) {
                            frames.push_back({&obj.children[obj_frames[static_cast<usize>(i)].index], obj_frames[static_cast<usize>(i)].node});
                        }
                    } else if constexpr (toml::is_array<T>) {
                        const toml::array& tarr = *f.table->as_array();
                        f.node->set<AData::Array>();
                        auto& arr = f.node->array();
                        arr.ensure(tarr.size());
                        for (usize i = 0; i < tarr.size(); ++i) {
                            frames.push_back({&arr[static_cast<std::ptrdiff_t>(i)], &tarr[i]});
                        }
                    }
                });
            }
        } catch (const toml::parse_error&) {
            return false;
        }
        return true;
    }

    static void populate_toml_table(toml::table& tbl, const AData::Object& obj);
    static void populate_toml_array(toml::array& arr, const AData::Array& a);

    static void populate_toml_table(toml::table& tbl, const AData::Object& obj) {
        for (auto proxy : obj) {
            const auto& node = proxy.second();
            std::string_view key = proxy.first();
            if (node.is_null()) {
                tbl.insert_or_assign(key, "");
            } else if (node.is_value()) {
                const auto& v = node.value();
                switch (v.get_type()) {
                    case Value::STRING:
                        tbl.insert_or_assign(key, v.to<std::string_view>());
                        break;
                    case Value::INT:
                        tbl.insert_or_assign(key, v.to<int64_t>());
                        break;
                    case Value::FLOATING:
                        tbl.insert_or_assign(key, v.to<double>());
                        break;
                    case Value::BOOL:
                        tbl.insert_or_assign(key, v.to<bool>());
                        break;
                }
            } else if (node.is_array()) {
                toml::array sub_arr;
                populate_toml_array(sub_arr, node.array());
                tbl.insert_or_assign(key, std::move(sub_arr));
            } else if (node.is_object()) {
                toml::table sub_tbl;
                populate_toml_table(sub_tbl, node.object());
                tbl.insert_or_assign(key, std::move(sub_tbl));
            }
        }
    }

    static void populate_toml_array(toml::array& arr, const AData::Array& a) {
        for (const auto& node : a) {
            if (node.is_null()) {
                arr.push_back("");
            } else if (node.is_value()) {
                const auto& v = node.value();
                switch (v.get_type()) {
                    case Value::STRING:
                        arr.push_back(v.to<std::string_view>());
                        break;
                    case Value::INT:
                        arr.push_back(v.to<int64_t>());
                        break;
                    case Value::FLOATING:
                        arr.push_back(v.to<double>());
                        break;
                    case Value::BOOL:
                        arr.push_back(v.to<bool>());
                        break;
                }
            } else if (node.is_array()) {
                toml::array sub_arr;
                populate_toml_array(sub_arr, node.array());
                arr.push_back(std::move(sub_arr));
            } else if (node.is_object()) {
                toml::table sub_tbl;
                populate_toml_table(sub_tbl, node.object());
                arr.push_back(std::move(sub_tbl));
            }
        }
    }

    void TOML::__internal_dump(__dump_fn fn, void* p, const AData& root) const {
        std::stringstream ss;
        if (root.is_object()) {
            toml::table tbl;
            populate_toml_table(tbl, root.object());
            ss << tbl;
        } else if (root.is_array()) {
            toml::array arr;
            populate_toml_array(arr, root.array());
            ss << arr;
        } else if (root.is_value()) {
            const auto& v = root.value();
            switch (v.get_type()) {
                case Value::STRING:
                    ss << toml::value<std::string>(v.to<std::string>());
                    break;
                case Value::INT:
                    ss << toml::value<int64_t>(v.to<int64_t>());
                    break;
                case Value::FLOATING:
                    ss << toml::value<double>(v.to<double>());
                    break;
                case Value::BOOL:
                    ss << toml::value<bool>(v.to<bool>());
                    break;
            }
        }
        std::string out = ss.str();
        fn(out, p);
    }

} // namespace alib6::data
