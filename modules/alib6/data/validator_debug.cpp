/**
 * @file validator_debug.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Schema 校验树调试输出实现 (alib6.data)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <iostream>

module alib6.data;
import std;
import alib6.core;

namespace pmr = std::pmr;

namespace alib6::data::debug {

    static std::string_view type_restrict_to_string(Validator::Node::TypeRestrict t) noexcept {
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

    static std::string get_indent(int depth) {
        return std::string(static_cast<usize>(depth * 4), ' ');
    }

    void print_validator(const Validator::Node& node, int depth, const std::string& label) {
        std::string indent = get_indent(depth);

        std::cout << indent << (label.empty() ? "[ROOT]" : label)
                  << " <" << type_restrict_to_string(node.type_restrict) << ">"
                  << (node.required ? " [REQUIRED]" : " [OPTIONAL]")
                  << "\n";

        auto min_s = node.min_length.to<std::string_view>();
        auto max_s = node.max_length.to<std::string_view>();

        if (!min_s.empty() || !max_s.empty()) {
            std::cout << indent << "  |-- Range: "
                      << (min_s.empty() ? "-inf" : min_s) << " ~ "
                      << (max_s.empty() ? "+inf" : max_s) << "\n";
        }

        if (!node.validates.empty()) {
            std::cout << indent << "  |-- Validates:\n";
            for (const auto& v : node.validates) {
                std::cout << indent << "      * " << v.method << "(";
                for (usize i = 0; i < v.args.size(); ++i) {
                    std::cout << v.args[i] << (i == v.args.size() - 1 ? "" : ", ");
                }
                std::cout << ")\n";
            }
        }

        if (node.default_value.has_value()) {
            std::cout << indent << "  |-- Default: " << node.default_value->str(JSON()) << "\n";
        }

        if (!node.array_subs.empty()) {
            std::cout << indent << "  |-- Array Subs (" << node.array_subs.size() << " items):\n";
            for (usize i = 0; i < node.array_subs.size(); ++i) {
                std::string sub_label = "[" + std::to_string(i) + "]";
                print_validator(node.array_subs[i], depth + 1, sub_label);
            }
        }

        if (!node.children.empty()) {
            std::cout << indent << "  |-- Children (" << node.children.size() << " keys):\n";
            for (const auto& [key, child] : node.children) {
                print_validator(child, depth + 1, "\"" + std::string(key) + "\"");
            }
        }
    }

} // namespace alib6::data::debug
