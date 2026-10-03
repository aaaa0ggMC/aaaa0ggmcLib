/**
 * @file validator.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Schema 结构校验器实现 (alib6.data)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cctype>
#include <cstddef>

module alib6.data;
import std;
import alib6.core;

namespace pmr = std::pmr;

namespace alib6::data {

    static std::pair<bool, Validator::Node::TypeRestrict> simp_validate_type(const AData& doc, const Validator::Node& node) {
        using enum Validator::Node::TypeRestrict;
        using enum AData::Type;
        using enum Value::Type;

        Validator::Node::TypeRestrict data_type{RNone}, ext{RNone};

        switch (doc.get_type()) {
            case TNull:
                data_type = RNull;
                break;
            case TArray:
                data_type = RArray;
                break;
            case TObject:
                data_type = RObject;
                break;
            case TValue:
                switch (doc.value().get_type()) {
                    case INT:      ext = RInt; break;
                    case FLOATING: ext = RDouble; break;
                    case BOOL:     ext = RBool; break;
                    case STRING:   ext = RString; break;
                }
                break;
        }

        if (node.type_restrict == RNone) {
            return {true, ext != RNone ? ext : data_type};
        }

        if (node.type_restrict == RValue && doc.is_value()) {
            return {true, ext != RNone ? ext : data_type};
        }

        if (data_type == node.type_restrict) {
            return {true, data_type};
        } else if (ext == node.type_restrict) {
            return {true, ext};
        }

        return {false, ext != RNone ? ext : data_type};
    }

    static std::string_view node_type_str(Validator::Node::TypeRestrict t) noexcept {
        switch (t) {
            case Validator::Node::RNone:   return "None";
            case Validator::Node::RNull:   return "Null";
            case Validator::Node::RValue:  return "Value";
            case Validator::Node::RInt:    return "Int";
            case Validator::Node::RDouble: return "Double";
            case Validator::Node::RBool:   return "Bool";
            case Validator::Node::RString: return "String";
            case Validator::Node::RArray:  return "Array";
            case Validator::Node::RObject: return "Object";
            default:                       return "Unknown";
        }
    }

    bool Validator::validate(AData& doc, Result& result, bool ignore_missing) {
        struct Frame {
            AData* d;
            Node* n;
            std::string_view key{""};
            int index{-1};
            // 整棵子树是否由"可选键缺席"自动补出的空壳派生:
            // 壳内只负责让子树里的 default_value 落地, 不强制任何 required 校验
            bool relaxed_missing{false};
        };

        using n_t = std::variant<std::string_view, usize>;
        std::vector<n_t> visit_tree;
        std::vector<Frame> frames;
        pmr::string gen_loc(allocator);
        bool success = true;

        struct ONext {
            usize index;
            std::string_view name;
            Node* n;
            bool relaxed_missing{false};
        };
        std::vector<ONext> object_next;
        frames.push_back({&doc, &root, "", -1});
        int depth = 0;

        while (!frames.empty()) {
            auto [d, n, key, index, relaxed_missing] = frames.back();
            frames.pop_back();

            if (d == nullptr) {
                if (!visit_tree.empty()) visit_tree.pop_back();
                --depth;
                continue;
            }

            auto get_vitree = [&]() -> std::string_view {
                gen_loc.clear();
                if (!visit_tree.empty() && visit_tree[0].index() == 1) {
                    gen_loc.push_back('[');
                }
                for (usize i = 0; i < visit_tree.size(); ++i) {
                    if (auto c = std::get_if<usize>(&visit_tree[i])) {
                        gen_loc += alib6::ext::to_string(*c, allocator);
                        gen_loc.push_back(']');
                    } else {
                        gen_loc += std::get<std::string_view>(visit_tree[i]);
                    }

                    if (i + 1 < visit_tree.size()) {
                        if (visit_tree[i + 1].index() == 1) gen_loc.push_back('[');
                        else gen_loc.push_back('.');
                    }
                }

                if (index >= 0) {
                    gen_loc.push_back('[');
                    gen_loc += alib6::ext::to_string(index, allocator);
                    gen_loc.push_back(']');
                } else if (!key.empty()) {
                    gen_loc.push_back('.');
                    gen_loc += key;
                }
                return gen_loc;
            };

            // relaxed_missing 模式: 整个子树是"可选键缺席"补出的空壳,
            // 跳过一切叶子级强制校验 (类型/枚举/长度/自定义), 直接下钻让子树里的 default_value 落地
            if (!relaxed_missing) {
            // 1. 默认值覆盖
            if (d->is_null() && n->default_value) {
                *d = *n->default_value;
            } else if (!d->is_null() || (n->array_subs.empty() && n->children.empty())) {
                auto type_check = simp_validate_type(*d, *n);
                if (!type_check.first) {
                    if (n->override_if_conflict && n->default_value) {
                        d->rewrite(*n->default_value);
                    } else {
                        if (result.enable_string_errors) {
                            result.record_error(
                                "{} : Expected type {}, got {}",
                                get_vitree(),
                                node_type_str(n->type_restrict),
                                node_type_str(type_check.second)
                            );
                        }
                        success = false;
                        continue;
                    }
                }
            } else {
                if (!n->array_subs.empty()) {
                    d->set<AData::Array>();
                } else if (!n->children.empty()) {
                    d->set<AData::Object>();
                }
            }

            // 2. 验证枚举
            if (d->is_value() && !n->enums.empty()) {
                auto val_str = d->value().to<std::string_view>();
                if (n->enums.find(val_str) == n->enums.end()) {
                    if (result.enable_string_errors) {
                        result.record_error(
                            "{} : Value \"{}\" does not match enum restriction",
                            get_vitree(),
                            val_str
                        );
                    }
                    success = false;
                    continue;
                }
            }

            // 3. 验证长度 / 数值区间
            if (!d->is_null() && (!d->is_value() || d->value().get_type() == Value::STRING)) {
                bool has_min = !n->min_length.to<std::string_view>().empty();
                bool has_max = !n->max_length.to<std::string_view>().empty();

                if (has_min || has_max) {
                    int min_l = has_min ? n->min_length.to<int>() : -1;
                    int max_l = has_max ? n->max_length.to<int>() : -1;

                    usize sz = 0;
                    if (d->is_array()) sz = d->array().size();
                    else if (d->is_object()) sz = d->object().size();
                    else sz = d->value().to<std::string_view>().size();

                    if ((min_l >= 0 && static_cast<int>(sz) < min_l) ||
                        (max_l >= 0 && static_cast<int>(sz) > max_l)) {
                        if (result.enable_string_errors) {
                            result.record_error(
                                "{} : Expected size [{}, {}], got {}",
                                get_vitree(),
                                (min_l >= 0) ? n->min_length.to<std::string>() : "0",
                                (max_l >= 0) ? n->max_length.to<std::string>() : "+inf",
                                sz
                            );
                        }
                        success = false;
                        continue;
                    }
                }
            } else if (d->is_value()) {
                const auto& val = d->value();
                bool has_min = !n->min_length.to<std::string_view>().empty();
                bool has_max = !n->max_length.to<std::string_view>().empty();
                if (has_min || has_max) {
                    double min_l = has_min ? n->min_length.to<double>() : -std::numeric_limits<double>::infinity();
                    double max_l = has_max ? n->max_length.to<double>() : std::numeric_limits<double>::infinity();
                    double actual = val.to<double>();
                    if (actual < min_l || actual > max_l) {
                        if (result.enable_string_errors) {
                            result.record_error(
                                "{} : Expected numeric range [{}, {}], got {}",
                                get_vitree(),
                                has_min ? n->min_length.to<std::string>() : "-inf",
                                has_max ? n->max_length.to<std::string>() : "+inf",
                                actual
                            );
                        }
                        success = false;
                        continue;
                    }
                }
            } else {
                continue;
            }

            // 4. 自定义 Validate 方法
            if (!n->validates.empty()) {
                bool fail = false;
                for (auto& method : n->validates) {
                    auto it = validates.find(method.method);
                    if (it != validates.end()) {
                        if (!it->second(*d, method.args)) {
                            fail = true;
                            if (result.enable_string_errors) {
                                result.record_error(
                                    "{} : Validation method '{}' failed",
                                    get_vitree(),
                                    method.method
                                );
                            }
                            break;
                        }
                    } else {
                        if (result.enable_missing) result.missing_validate(method.method);
                    }
                }
                if (fail) {
                    success = false;
                    continue;
                }
            }
            }

            auto push_vis = [&] {
                if (index >= 0) visit_tree.emplace_back(static_cast<usize>(index));
                else visit_tree.emplace_back(key);
            };

            // 5. 下钻遍历
            if (d->is_array()) {
                if (n->is_tuple) {
                    auto& arr = d->array();
                    push_vis();
                    ++depth;
                    frames.push_back({nullptr, nullptr, "", -1});
                    for (usize i = 0; i < arr.size(); ++i) {
                        if (i >= n->array_subs.size()) break;
                        if (arr[static_cast<std::ptrdiff_t>(i)].is_null() && n->array_subs[i].default_value) {
                            arr[static_cast<std::ptrdiff_t>(i)] = *n->array_subs[i].default_value;
                        } else {
                            frames.push_back({&arr[static_cast<std::ptrdiff_t>(i)], &n->array_subs[i], "", static_cast<int>(i), relaxed_missing});
                        }
                    }
                } else if (!n->array_subs.empty()) {
                    push_vis();
                    ++depth;
                    auto& arr = d->array();
                    frames.push_back({nullptr, nullptr, "", -1});
                    for (usize i = 0; i < arr.size(); ++i) {
                        if (arr[static_cast<std::ptrdiff_t>(i)].is_null() && n->array_subs[0].default_value) {
                            arr[static_cast<std::ptrdiff_t>(i)] = *n->array_subs[0].default_value;
                        } else {
                            frames.push_back({&arr[static_cast<std::ptrdiff_t>(i)], &n->array_subs[0], "", static_cast<int>(i), relaxed_missing});
                        }
                    }
                }
            } else if (d->is_object()) {
                auto& obj = d->object();
                bool fail = false;
                ++depth;
                frames.push_back({nullptr, nullptr, "", -1});

                object_next.clear();
                for (auto& [k, v] : n->children) {
                    auto it = obj.find(k);
                    if (it == obj.end()) {
                        if (!ignore_missing) {
                            if (v.default_value) {
                                obj[k] = *v.default_value;
                                continue;
                            }

                            if (v.type_restrict == Node::RArray || v.type_restrict == Node::RObject) {
                                // 结构化键缺席: 补壳并下钻, 让子树里的 default_value 有机会落地
                                // - required 键保持严格 (孙级缺 required 照常报错), 与历史行为一致
                                // - optional 键 / 已处于 relaxed 子树内: 整棵 relaxed, 只填默认不强制
                                bool child_relaxed = relaxed_missing || !v.required;
                                if (v.type_restrict == Node::RArray) {
                                    obj[k].set<AData::Array>();
                                } else {
                                    obj[k].set<AData::Object>();
                                }
                                auto new_it = obj.find(k);
                                object_next.push_back({new_it.it->second, k, &v, child_relaxed});
                                continue;
                            }

                            // 标量键缺席
                            if (v.required && !relaxed_missing) {
                                if (result.enable_string_errors) {
                                    result.record_error(
                                        "{} : Required child '{}' is missing",
                                        get_vitree(),
                                        k
                                    );
                                }
                                fail = true;
                                success = false;
                                break;
                            }
                            // optional 或 relaxed: 静默跳过, 交由 C++ 侧默认成员初始化兜底
                        }
                    } else {
                        // 在场键: 继承当前帧的 relaxed 状态
                        object_next.push_back({it.it->second, k, &v, relaxed_missing});
                    }
                }

                push_vis();

                if (fail) continue;
                for (auto& i : object_next) {
                    frames.push_back({&obj.children[i.index], i.n, i.name, -1, i.relaxed_missing});
                }
            }
        }

        result.success = success;
        return success;
    }

    pmr::string Validator::from_adata(const AData& doc) {
        pmr::string errors(allocator);
        Node restriction(allocator);

        struct Job {
            const AData* d;
            Node* n;
            pmr::string visit_tree;
        };
        std::vector<Job> clayer, nlayer;

        alib6::Parser parser(allocator);
        std::string bg_parse;

        auto enum_restrict = [&](std::string_view visit_tree) -> bool {
            if (restriction.type_restrict == Node::RNone || restriction.type_restrict == Node::RValue) {
                restriction.type_restrict = Node::RString;
            } else if (restriction.type_restrict != Node::RString) {
                std::format_to(std::back_inserter(errors), "Only StringType is allowed for an enum. VISIT_TREE \"{}\"\n", visit_tree);
                return false;
            }
            return true;
        };

        auto parse_restriction = [&](std::string_view str, std::string_view visit_tree) -> bool {
            parser.parse(str);
            restriction.reset();
            auto analyser = parser.analyse();
            auto cursor = analyser.as_cursor();
            auto current = cursor.head();
            if (current.view().empty()) return true;
            cursor.matched = true;

            constexpr std::string_view lb = "(";
            constexpr std::string_view rb = ")";

            while (true) {
                bg_parse.clear();
                for (char ch : current.view()) bg_parse.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));

                if (bg_parse == "OPTIONAL") {
                    restriction.required = false;
                } else if (bg_parse == "REQUIRED") {
                    restriction.required = true;
                } else if (bg_parse == "OVERRIDE_CONFLICT") {
                    restriction.override_if_conflict = true;
                } else if (bg_parse == "TYPE") {
                    auto v = cursor.next();
                    if (v) {
                        bg_parse.clear();
                        for (char ch : v.view()) bg_parse.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                        if (bg_parse == "NONE") restriction.type_restrict = Node::RNone;
                        else if (bg_parse == "NULL") restriction.type_restrict = Node::RNull;
                        else if (bg_parse == "INT") restriction.type_restrict = Node::RInt;
                        else if (bg_parse == "DOUBLE") restriction.type_restrict = Node::RDouble;
                        else if (bg_parse == "STRING") restriction.type_restrict = Node::RString;
                        else if (bg_parse == "VALUE") restriction.type_restrict = Node::RValue;
                        else if (bg_parse == "OBJECT") restriction.type_restrict = Node::RObject;
                        else if (bg_parse == "ARRAY") restriction.type_restrict = Node::RArray;
                        else if (bg_parse == "BOOL") restriction.type_restrict = Node::RBool;
                        else {
                            std::format_to(std::back_inserter(errors), "Unknown type \"{}\" when parsing TYPE! VISIT_TREE \"{}\"\n", v.view(), visit_tree);
                            return false;
                        }
                    } else {
                        std::format_to(std::back_inserter(errors), "Empty typename when parsing TYPE! VISIT_TREE \"{}\"\n", visit_tree);
                        return false;
                    }

                    if (!restriction.enums.empty()) {
                        if (!enum_restrict(visit_tree)) return false;
                    }
                } else if (bg_parse == "VALIDATE") {
                    auto v = cursor.next();
                    if (!v) {
                        std::format_to(std::back_inserter(errors), "Empty method when parsing VALIDATE VISIT_TREE: {}\n", visit_tree);
                        return false;
                    }
                    auto& vl = restriction.validates.emplace_back(allocator);
                    vl.method = activated_validates_str_pool.get(v.view());
                    if (cursor.peek().view() == lb) {
                        cursor.next();
                        while (!cursor.reached_end()) {
                            auto item = cursor.next();
                            if (item.view() == rb) break;
                            vl.args.emplace_back(item.view());
                        }
                    }
                } else if (bg_parse == "ENUM") {
                    if (!enum_restrict(visit_tree)) return false;
                    if (cursor.peek().view() == lb) {
                        cursor.next();
                        while (!cursor.reached_end()) {
                            auto item = cursor.next();
                            if (item.view() == rb) break;
                            restriction.enums.emplace(item.view());
                        }
                    } else {
                        std::format_to(std::back_inserter(errors), "Syntax error for ENUM at VISIT_TREE \"{}\"\n", visit_tree);
                        return false;
                    }
                } else if (bg_parse == "MIN" || bg_parse == "MAX") {
                    Value* op = (bg_parse == "MIN") ? &restriction.min_length : &restriction.max_length;
                    auto c = cursor.next();
                    if (c.view().empty()) {
                        std::format_to(std::back_inserter(errors), "Empty value for MIN/MAX at VISIT_TREE \"{}\"\n", visit_tree);
                        return false;
                    }
                    auto val = c.expect<double>();
                    if (!val) {
                        std::format_to(std::back_inserter(errors), "Invalid value \"{}\" for MIN/MAX at VISIT_TREE \"{}\"\n", c.view(), visit_tree);
                        return false;
                    }
                    *op = c.view();
                }

                current = cursor.next();
                if (current.view().empty()) break;
            }

            if (!restriction.max_length.to<std::string_view>().empty() &&
                !restriction.min_length.to<std::string_view>().empty()) {
                double min_v = restriction.min_length.to<double>();
                double max_v = restriction.max_length.to<double>();
                if (min_v > max_v) {
                    std::format_to(std::back_inserter(errors), "MIN value {} is greater than MAX value {} at VISIT_TREE: {}\n", min_v, max_v, visit_tree);
                    return false;
                }
            }
            return true;
        };

        clayer.push_back({&doc, &root, pmr::string(allocator)});

        while (!clayer.empty()) {
            while (!clayer.empty()) {
                auto [d, current, visit_tree] = std::move(clayer.back());
                clayer.pop_back();

                switch (d->get_type()) {
                    case AData::TNull:
                        break;
                    case AData::TValue:
                        if (parse_restriction(d->value().to<std::string_view>(), visit_tree)) {
                            *current = restriction;
                        }
                        break;
                    case AData::TArray: {
                        const auto& arr = d->array();
                        if (arr.empty()) {
                            std::format_to(std::back_inserter(errors), "Array size cannot be lower than 1 when parsing schema array! VISIT_TREE {}\n", visit_tree);
                            continue;
                        }

                        const auto& val = arr.values;
                        usize vi_offset = 0;
                        if (!val[0].is_value()) {
                            std::format_to(std::back_inserter(errors), "Array[0] should be Value when parsing schema array! VISIT_TREE {}\n", visit_tree);
                            continue;
                        }

                        if (arr.size() == 1) {
                            if (parse_restriction(val[0].value().to<std::string_view>(), visit_tree)) {
                                *current = restriction;
                            }
                            break;
                        }

                        bg_parse.clear();
                        for (char ch : val[0].value().to<std::string_view>()) bg_parse.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                        if (bg_parse != "LIST" && bg_parse != "TUPLE") {
                            vi_offset = 1;
                            bg_parse = "LIST";
                        }

                        if (bg_parse == "LIST") {
                            if (!val[1 - vi_offset].is_value()) {
                                std::format_to(std::back_inserter(errors), "First constraint should be Value in LIST schema! VISIT_TREE {}\n", visit_tree);
                                continue;
                            }
                            if (!parse_restriction(val[1 - vi_offset].to<std::string_view>(), visit_tree)) continue;
                            *current = restriction;

                            if (val.size() >= 3 - vi_offset) {
                                if (current->type_restrict != Node::RArray) {
                                    if (simp_validate_type(val[2 - vi_offset], *current).first) {
                                        if (!current->enums.empty() &&
                                            current->enums.find(val[2 - vi_offset].to<std::string_view>()) == current->enums.end()) {
                                            std::format_to(std::back_inserter(errors), "Default value \"{}\" does not match enum restriction at VISIT_TREE {}\n", val[2 - vi_offset].to<std::string_view>(), visit_tree);
                                            continue;
                                        }
                                        current->default_value = val[2 - vi_offset];
                                    } else {
                                        std::format_to(std::back_inserter(errors), "Default value does not match type restriction \"{}\" at VISIT_TREE {}\n", node_type_str(current->type_restrict), visit_tree);
                                        continue;
                                    }
                                } else {
                                    auto next_vi = visit_tree;
                                    next_vi += "[default]";
                                    nlayer.push_back({&val[2 - vi_offset], &current->array_subs.emplace_back(allocator), std::move(next_vi)});
                                }
                            }
                        } else if (bg_parse == "TUPLE") {
                            current->type_restrict = Node::RArray;
                            current->min_length = static_cast<i64>(val.size() - 1);
                            current->max_length = static_cast<i64>(val.size() - 1);
                            current->is_tuple = true;
                            current->array_subs.reserve(val.size() - 1);
                            for (usize i = 1; i < val.size(); ++i) {
                                auto next_vi = visit_tree;
                                next_vi += "[";
                                next_vi += alib6::ext::to_string(i, allocator);
                                next_vi += "]";
                                nlayer.push_back({&val[i], &current->array_subs.emplace_back(allocator), std::move(next_vi)});
                            }
                        }
                        break;
                    }
                    case AData::TObject: {
                        const auto& obj = d->object();
                        auto magic_schema = obj.find(magic_key_for_schema_restr);
                        if (magic_schema == obj.end()) {
                            magic_schema = obj.find(magic_key_for_schema_restr_legacy);
                        }

                        if (magic_schema != obj.end()) {
                            if (magic_schema.second().get_type() != AData::TValue) {
                                std::format_to(std::back_inserter(errors), "Only string is allowed for object description at VISIT_TREE {}\n", visit_tree);
                                continue;
                            }
                            if (!parse_restriction(magic_schema.second().to<std::string_view>(), visit_tree)) continue;
                            *current = restriction;
                        }

                        if (current->type_restrict == Node::RNone) {
                            current->type_restrict = Node::RObject;
                        } else if (current->type_restrict != Node::RObject) {
                            std::format_to(std::back_inserter(errors), "Object cannot be restricted to non-object type \"{}\" at VISIT_TREE {}\n", node_type_str(current->type_restrict), visit_tree);
                            continue;
                        }

                        for (auto proxy : obj) {
                            if (proxy.first() == magic_key_for_schema_restr || proxy.first() == magic_key_for_schema_restr_legacy) {
                                continue;
                            }
                            auto next_vi = visit_tree;
                            next_vi += ".";
                            next_vi += proxy.first();
                            nlayer.push_back({&proxy.second(), &current->children.try_emplace(proxy.first(), allocator).first->second, std::move(next_vi)});
                        }
                        break;
                    }
                }
            }
            clayer = std::move(nlayer);
            nlayer.clear();
        }

        return errors;
    }

} // namespace alib6::data
