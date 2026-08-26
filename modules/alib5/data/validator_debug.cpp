#include <alib5/adata.h>

using namespace alib5;

static std::string type_restrict_to_string(Validator::Node::TypeRestrict t) {
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
        default:            return "Unknown";
    }
}

// 辅助函数：生成缩进
static std::string get_indent(int depth) {
    return std::string(depth * 4, ' ');
}

/**
 * 递归打印 Node 信息
 * @param node 节点引用
 * @param depth 当前深度
 * @param label 当前节点的名称/Key（如果是根节点或数组元素，可以为空或索引）
 */
void debug::print_validator(const Validator::Node& node, int depth, const std::string& label){
    std::string indent = get_indent(depth);
    
    // 1. 打印头部信息 (Key + Type + Required)
    std::cout << indent << (label.empty() ? "[ROOT]" : label) 
              << " <" << type_restrict_to_string(node.type_restrict) << ">"
              << (node.required ? " [REQUIRED]" : " [OPTIONAL]") 
              << "\n";

    // 2. 打印限制条件 (Min/Max)
    // 假设 dvalue_t 有 to<std::string>() 或者类似的转字符串方法
    std::string_view min_s = node.min_length.to<std::string>();
    std::string_view max_s = node.max_length.to<std::string>();
    
    if (!min_s.empty() || !max_s.empty()) {
        std::cout << indent << "  |-- Range: " 
                  << (min_s.empty() ? "-inf" : min_s) << " ~ " 
                  << (max_s.empty() ? "+inf" : max_s) << "\n";
    }

    // 3. 打印校验器 (Validators)
    if (!node.validates.empty()) {
        std::cout << indent << "  |-- Validates:\n";
        for (const auto& v : node.validates) {
            std::cout << indent << "      * " << v.method << "(";
            for (size_t i = 0; i < v.args.size(); ++i) {
                std::cout << v.args[i] << (i == v.args.size() - 1 ? "" : ", ");
            }
            std::cout << ")\n";
        }
    }

    // 4. 打印默认值 (Default Value)
    if (node.default_value.has_value()) {
        // 注意：这里假设 AData 可以被序列化为字符串
        // 如果 AData 是复杂对象，你可能需要调用 d.to_json() 或 d.to_string()
        // 这里只是示意：
        // std::string def_str = "TODO: AData_to_String"; 
        // def_str = node.default_value->to_string(); // <--- 替换为你实际的API
        
        std::cout << indent << "  |-- Default: " << node.default_value->str() << "\n";
    }

    // 5. 递归打印子节点 (Array Subs)
    if (!node.array_subs.empty()) {
        std::cout << indent << "  |-- Array Subs (" << node.array_subs.size() << " items):\n";
        for (size_t i = 0; i < node.array_subs.size(); ++i) {
            std::string sub_label = "[" + std::to_string(i) + "]";
            print_validator(node.array_subs[i], depth + 1, sub_label);
        }
    }

    // 6. 递归打印子节点 (Children / Map)
    if (!node.children.empty()) {
        std::cout << indent << "  |-- Children (" << node.children.size() << " keys):\n";
        for (const auto& [key, child] : node.children) {
            // key 是 pmr::string，可以直接输出
            print_validator(child, depth + 1, "\"" + std::string(key) + "\"");
        }
    }
}