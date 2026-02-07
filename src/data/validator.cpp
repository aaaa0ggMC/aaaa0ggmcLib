#include <alib5/adata.h>

using namespace alib5;

static std::pair<bool,Validator::Node::TypeRestrict> simp_validate_type(const AData & doc,Validator::Node & node){
    using enum Validator::Node::TypeRestrict;
    using enum AData::Type;
    using enum Value::Type;
    Validator::Node::TypeRestrict data_type {RNone},ext {RNone};
    switch(doc.get_type()){
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
        switch(doc.value().get_type()){
        case INT:
            ext = RInt;
            break;
        case FLOATING:
            ext = RDouble;
            break;
        case BOOL:
            ext = RBool;
            break;
        case STRING:
            ext = RString;
            break;
        }
        break;
    }
    if(node.type_restrict == RNone)return {true,ext != RNone ? ext : data_type};

    if(data_type == node.type_restrict){
        return { true , data_type};
    }else if(ext == node.type_restrict){
        return { true , ext};
    }
    return {false,ext != RNone ? ext : data_type};
}

static std::string_view node_type_str(Validator::Node::TypeRestrict t){
    switch(t){
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
bool ALIB5_API Validator::validate(AData & doc,Result & result){
    // 注意,对于数组以及Object处理都要格外小心,因为都是线性存储,小心引用失效!
    // 好在DFS只会影响最深一层的引用
    // 因此安全系数也不是那么差
    struct Frame{
        AData * d;
        Node * n;
        std::string_view key { "" };
        int index { -1 };
    };
    using n_t = std::variant<std::string_view,size_t>;
    std::vector<n_t> visit_tree;
    std::vector<Frame> frames;    
    std::pmr::string gen_loc(allocator);
    bool success = true;
    struct ONext{
        size_t index;
        std::string_view name;
        Node * n;
    };
    std::vector<ONext> object_next;
    frames.emplace_back(&doc,&root);
    int depth = 0;

    while(!frames.empty()){
        auto [d,n,key,index] = frames.back();
        frames.pop_back();

        if (d == nullptr) {
            if(!visit_tree.empty())visit_tree.pop_back();
            --depth;
            continue;
        }

        auto get_vitree = [&] -> std::string_view{
            gen_loc.clear();
            if(visit_tree.size() && visit_tree[0].index() == 1){
                gen_loc.push_back('[');
            }
            for(size_t i = 0;i < visit_tree.size();++i){
                if(auto c = std::get_if<size_t>(&visit_tree[i])){
                    gen_loc += ext::to_string(*c);
                    gen_loc.push_back(']');
                }else{
                    gen_loc += std::get<std::string_view>(visit_tree[i]);
                }
                
                if(i+1 < visit_tree.size()){
                    if(visit_tree[i+1].index() == 1)gen_loc.push_back('[');
                    else gen_loc.push_back('.');
                }
            }

            if(index >= 0){
                gen_loc.push_back('[');
                gen_loc += ext::to_string(index);
                gen_loc.push_back(']');
            }else{
                gen_loc.push_back('.');
                gen_loc += key;
            }
            return gen_loc;
        };

        // 默认值覆盖
        if(d->is_null() && n->default_value){
            /// 因为隐式转换静止,因此确保一下是NULL 
            *d = *n->default_value;
        }else if(!d->is_null() || (!n->array_subs.size() && !n->children.size())){
            auto type_check = simp_validate_type(*d, *n);
            // std::cout << n->min_length.to<std::string>() << "DUMP" << d->str() << "DUMP\n" << std::endl;       
            // std::cout << "MATCHING " << 
            //        node_type_str(n->type_restrict) << " " <<
            //        node_type_str(type_check.second) << std::endl;
            if(!type_check.first){
                // std::cout << n->override_if_conflict << std::endl;
                if(n->override_if_conflict && n->default_value){
                    d->rewrite(*n->default_value);
                }else{
                    if(result.enable_string_errors)result.record_error(
                        "{} : Expected type {},got {}",
                        get_vitree(),
                        node_type_str(n->type_restrict),
                        node_type_str(type_check.second)
                    );
                    success = false;
                    continue;
                }
            }
        }else{
            // std::cout << "GENNNN" << std::endl;
            // NULL生万物
            if(n->array_subs.size()){
                d->set<darray_t>();
            }else if(n->children.size()){
                d->set<dobject_t>();
            }
        }
        // 验证长度
        if(!d->is_null() && (!d->is_value() || d->value().get_type() == Value::STRING)){
            // 这里就是数组/对象长度了
            bool has_min = n->min_length.to<std::string_view>() != "";
            bool has_max = n->max_length.to<std::string_view>() != "";
            
            if(has_min || has_max){
                int min_l = has_min ? n->min_length.to<int>() : -1;
                int max_l = has_max ? n->max_length.to<int>() : -1;
            
                auto sz = 0;
                if(d->is_array())sz = d->array().size();
                else if(d->is_object())sz = d->object().size();
                else sz = d->value().to<std::string_view>().size();

                // std::cout << "HAS " << min_l << std::endl;
                // std::cout << "HAS " << max_l << std::endl;
                if((min_l >= 0 && sz < min_l) ||
                (max_l >= 0 && sz > max_l)
                ){
                    // 这里还有array的默认值填充呢
                    if(d->is_array() && sz < min_l){
                        d->array().ensure(min_l);
                        // std::cout << "RESIZED " << min_l << std::endl;
                    }else{
                        if(result.enable_string_errors)result.record_error(
                            "{} : Expected target size [{},{}],got {}",
                            get_vitree(),
                            (min_l>=0) ? n->min_length.to<std::string>() : "0",
                            (max_l>=0) ? n->max_length.to<std::string>() : "+inf",
                            sz
                        );
                        success = false;
                        continue;
                    }
                }
            }
        }else if(d->is_value()){
            // 直接转换成double统一判断?
            auto & val = d->value();
            bool has_min = n->min_length.to<std::string_view>() != "";
            bool has_max = n->max_length.to<std::string_view>() != "";
            if(has_min || has_max){
                double min_l = has_min ? n->min_length.to<double>() : 0;
                double max_l = has_max ? n->max_length.to<double>() : 0;
                double actual = val.to<double>();
                if((has_min && actual < min_l) ||
                    (has_max && actual > max_l)
                ){
                    if(result.enable_string_errors)result.record_error(
                        "{} : Expected target size [{},{}],got {}",
                        get_vitree(),
                        (has_min) ? n->min_length.to<std::string>() : "-inf",
                        (has_max) ? n->max_length.to<std::string>() : "+inf",
                        actual
                    );
                    success = false;
                    continue;
                }
            }
        }else{
            // 都是null了,后面的内容我觉得没必要校验了...
            continue;
        }

        /// Validate校验
        if(n->validates.size()){
            bool fail = false;
            for(auto & method : n->validates){
                auto it = validates.find(method.method);
                if(it != validates.end()){
                    if(!it->second(*d,method.args)){
                        fail = true;
                        break;
                    }
                }else{
                    if(result.enable_missing)result.missing_validate(method.method);
                }
            }
        }

        auto push_vis = [&]{
            if(index >= 0)visit_tree.emplace_back((size_t)index);
            else visit_tree.emplace_back(key);
        };

        /// 装载新的数据
        if(d->is_array()){
            if(n->is_tuple){ // 说明每个rule都是不一致的
                auto & arr = d->array();
                push_vis();
                ++depth;
                frames.emplace_back(nullptr,nullptr);
                for(size_t i = 0;i < arr.size();++i){
                    // 在tuple中理论上是不会出现这个问题的
                    [[unlikely]] if(i >= n->array_subs.size()){
                        panic("不该运行到这里的");
                        break;
                    }
                    // 如果arr[i]为NULL并且array_subs[i]存在默认值,那么填充
                    if(arr[i].is_null() && n->default_value){
                        arr[i] = *n->array_subs[i].default_value;
                    }else frames.emplace_back(&arr[i],&n->array_subs[i],"",i);
                }
            }else{
                push_vis();
                ++depth;
                auto & arr = d->array();
                frames.emplace_back(nullptr,nullptr);
                // 第二个作为每个child的"rule"
                for(size_t i = 0;i < arr.size();++i){
                    if(arr[i].is_null() && n->array_subs[0].default_value){
                        arr[i] = *n->array_subs[0].default_value;
                    }else frames.emplace_back(&arr[i],&n->array_subs[0],"",i);
                }
            }
        }else if(d->is_object()){
            auto & obj = d->object();
            bool fail = false;
            ++depth;
            frames.emplace_back(nullptr,nullptr);
            
            object_next.clear();
            // 遍历元素,然后步进,顺便检查REQUIRED标记与DEFAULT VALUE自动PUSH
            for(auto & [k,v] : n->children){
                auto it = obj.find(k);
                // std::cout << "SEARCHING " << k << std::endl;
                if(it == obj.end()){
                    if(v.default_value){
                        obj[k] = *v.default_value;
                        // std::cout << "HAS DEFAULT" << std::endl;
                        continue;
                    }else if(v.type_restrict == Node::RArray){
                        // std::cout << "GENN ARR " << k << std::endl;
                        obj[k].set<darray_t>();
                        auto it = obj.find(k);
                        object_next.emplace_back(it.it->second,k,&v);
                    }else if(v.type_restrict == Node::RObject){
                        obj[k].set<dobject_t>();
                        // 因为上面的旧的it不支持copy operator
                        auto it = obj.find(k);
                        object_next.emplace_back(it.it->second,k,&v);
                    }else if(v.required){
                        if(result.enable_string_errors)result.record_error(
                            "{} : Required child {},but missing",
                            get_vitree(),
                            k
                        );
                        fail = true;
                        success = false;
                        break;
                    }
                }else{
                    // 先不急,这个也要确保引用一致
                    object_next.emplace_back(it.it->second,k,&v);
                }
            }
            /// 因为前面有输出,所以push_vis需要靠后
            push_vis();
           
            if(fail)continue;
            for(auto & i : object_next){
                frames.emplace_back(&obj.children[i.index],i.n,i.name,-1);
            }
        }
    }

    result.success = success;

    return success;
}
    

std::pmr::string Validator::from_adata(const AData & doc){
    std::pmr::string errors (allocator);
    Node restriction (allocator);

    /// BFS同步
    struct Job{
        const AData* d;
        Node* n;
        std::pmr::string visit_tree;
    };
    std::vector<Job> clayer,nlayer; 

    Parser parser;
    std::string bg_parse;
    auto parse_restriction = [&](std::string_view str,std::string_view visit_tree)->bool{
        parser.parse(str);
        // 重新设置默认值
        restriction.reset();
        auto analyser = parser.analyse();
        auto cursor = analyser.as_cursor();
        auto current = cursor.head();
        if(current.view() == "")return true;
        // 这里强行修复单token问题
        cursor.matched = true;
            
        constexpr std::string_view lb = "(";
        constexpr std::string_view rb = ")";

        while(true){
            bg_parse.clear();
            // std::cout << current.view() << std::endl;
            for(auto ch : current.view())bg_parse.push_back(std::toupper(ch));
            if(bg_parse == "OPTIONAL"){
                restriction.required = false;
                // std::cout << "OPTIONAL OCCURRED" << std::endl;
            }else if(bg_parse == "REQUIRED"){
                // 这个也加入识别可以强调语义
                restriction.required = true;
            }else if(bg_parse == "OVERRIDE_CONFLICT"){
                restriction.override_if_conflict = true;
            }else if(bg_parse == "TYPE"){
                auto v = cursor.next();
                if(v){
                    bg_parse.clear();
                    for(auto ch : v.view())bg_parse.push_back(std::toupper(ch));
                    if(bg_parse == "NONE")restriction.type_restrict = restriction.RNone;
                    else if(bg_parse == "NULL")restriction.type_restrict = restriction.RNull;
                    else if(bg_parse == "INT")restriction.type_restrict = restriction.RInt;
                    else if(bg_parse == "DOUBLE")restriction.type_restrict = restriction.RDouble;
                    else if(bg_parse == "STRING")restriction.type_restrict = restriction.RString;
                    else if(bg_parse == "VALUE")restriction.type_restrict = restriction.RValue;
                    else if(bg_parse == "OBJECT")restriction.type_restrict = restriction.RObject;
                    else if(bg_parse == "ARRAY")restriction.type_restrict = restriction.RArray;
                    else if(bg_parse == "BOOL")restriction.type_restrict = restriction.RBool;
                    else{
                        std::format_to(std::back_inserter(errors),
                            "Unknown type \"{}\" when parsing array! VISIT_TREE \"{}\"",
                            v.view(),
                            visit_tree
                        );
                        return false;
                    }
                }else{
                    std::format_to(std::back_inserter(errors),
                        "Empty typename when parsing TYPE! VISIT_TREE \"{}\"",
                        visit_tree
                    );
                    return false;
                }
            }else if(bg_parse == "VALIDATE"){
                auto v = cursor.next();
                if(!v){
                    std::format_to(std::back_inserter(errors),
                        std::runtime_format("Empty method when parsing VALIDATE VISIT_TREE:{}\n"),
                        visit_tree
                    );
                    return false;
                }
                auto& vl = restriction.validates.emplace_back(allocator);
                vl.method = activated_validates_str_pool.get(v.view());
                // 说明存在参数
                if(cursor.peek().view() == lb){
                    auto v = cursor.next();
                    while(!cursor.reached_end()){
                        v = cursor.next();
                        if(v.view() == rb)break;
                        vl.args.emplace_back(
                            std::pmr::string(v.view(),allocator)
                        );
                    }
                    if(v.view() != rb){
                        std::format_to(std::back_inserter(errors),
                            std::runtime_format("Unclosed args when parsing VALIDATE VISIT_TREE:{}\n"),
                            visit_tree
                        );
                        return false;
                    }
                }
            }else if(bg_parse == "MIN" || bg_parse == "MAX"){
                dvalue_t * op = nullptr;
                if(bg_parse == "MIN")op = &restriction.min_length;
                else op = &restriction.max_length;

                auto c = cursor.next();
                if(c.view() == ""){
                    std::format_to(std::back_inserter(errors),
                        std::runtime_format("Empty value when parsing MIN VISIT_TREE:{}\n"),
                        visit_tree
                    );
                    return false;
                }
                auto val = c.expect<double>();
                if(!val){
                    std::format_to(std::back_inserter(errors),
                        "Invalid value \"{}\" when parsing MIN VISIT_TREE:{}\n",
                        c.view(),
                        visit_tree
                    );
                    return false;
                }
                // 保存原始数据,后面to<double> to<int> ...
                *op = c.view();
            }else{
                // ignore useless tokens
            }
            current = cursor.next();
            if(current.view() == "")break;
        }
        /// 二次校验大小
        if(restriction.max_length.to<const char*>() != "" &&
            restriction.min_length.to<const char*>() != ""
        ){
            double min_v = restriction.min_length.to<double>();
            double max_v = restriction.max_length.to<double>();

            if(min_v > max_v){
                std::format_to(std::back_inserter(errors),
                    "MIN value \"{}\" is greater than MAX value \"{}\" VISIT_TREE:{}\n",
                    min_v,
                    max_v,
                    visit_tree
                );
                return false;
            }
        }
        return true;
    };

    clayer.emplace_back(&doc,&root,std::move(std::pmr::string(allocator)));
    while(!clayer.empty()){
        while(!clayer.empty()){
            auto [d,current,visit_tree] = std::move(clayer.back());
            clayer.pop_back();

            // std::cout << "VISITING " << visit_tree << std::endl;

            // 不会出现深层校验
            switch(d->get_type()){
            case dtype_t::TNull:
                // 为什么会传入NULL?,直接skip   
                break;
            case dtype_t::TValue:
                // 如果只是Value校验说明没提供默认值只是进行了约束
                if(parse_restriction(d->value().to<std::string>(),visit_tree))*current = restriction;
                else continue;
                break;
            case dtype_t::TArray:{
                auto & arr = d->array();
                if(arr.size() < 2){
                    std::format_to(std::back_inserter(errors),
                        "Array size cannot lower than 2 when parsing array! VISIT_TREE {}",
                        visit_tree
                    );
                    continue;
                }
                auto & val = arr.values;
                int vi_offset = 0;
                if(!val[0].is_value()){
                    std::format_to(std::back_inserter(errors),
                        "Array[0] should be value when parsing array! VISIT_TREE {}",
                        visit_tree
                    );
                    continue;
                }
                bg_parse.clear();
                for(auto ch : val[0].value().to<std::string>())bg_parse.push_back(std::toupper(ch));
                if(bg_parse != "LIST" && bg_parse != "TUPLE"){
                    vi_offset = 1;
                    bg_parse = "LIST";
                }
                if(bg_parse == "LIST"){
                    /// 这里对 value 1 为Value
                    if(!val[1 - vi_offset].is_value()){
                        std::format_to(std::back_inserter(errors),
                            "First restraint should be value when parsing array! VISIT_TREE {}",
                            visit_tree
                        );
                    }
                    // std::cout << "PARSING RESTR FOR LIST " << std::endl;
                    bool val1_parse = parse_restriction(val[1 - vi_offset].to<std::string>(),visit_tree);
                    *current = restriction;
                    /// 值区间 || 对象,第二个参数是默认值
                    if(val.size() >= 3 - vi_offset){
                        if(current->type_restrict != Node::RArray){
                            if(simp_validate_type(val[2 - vi_offset],*current).first)current->default_value = val[2 - vi_offset];
                            else{
                                std::format_to(std::back_inserter(errors),
                                    std::runtime_format("Default value doesnt match the restriction \"{}\" VISIT_TREE {}"),
                                    node_type_str(current->type_restrict),
                                    visit_tree
                                );
                            }
                        }else{ // Array递归描述
                            auto next_vi = visit_tree;
                            next_vi += "[default]";
                            nlayer.emplace_back(&val[2 - vi_offset],&current->array_subs.emplace_back(allocator),
                                std::move(next_vi)
                            );
                        }
                    }
                }else if(bg_parse == "TUPLE"){
                    // 这里就是对每个元素进行递归描述
                    // 对大小要求严谨
                    current->type_restrict = Node::RArray;
                    current->min_length = val.size() - 1;
                    current->max_length = val.size() - 1;
                    current->is_tuple = true;
                    // 防止扩容
                    current->array_subs.reserve(val.size());
                    for(size_t i = 1;i < val.size();++i){
                        auto next_vi = visit_tree;
                        next_vi += "[";
                        next_vi += ext::to_string(i);
                        next_vi += "]";
                        nlayer.emplace_back(&val[i],&current->array_subs.emplace_back(allocator),
                            std::move(next_vi)
                        );
                    }
                }
                break;
            }
            case dtype_t::TObject:
                // 这里理论上要加入对子类的验证,但是需要等等我
                current->type_restrict = Node::RObject;
                for(auto proxy : d->object()){
                    auto next_vi = visit_tree;
                    next_vi += ".";
                    next_vi += proxy.first;
                    nlayer.emplace_back(&proxy.second,&current->children.emplace(proxy.first,allocator).first->second,
                        std::move(next_vi)
                    );
                }
                break;
            }
        }

        clayer = std::move(nlayer);
        nlayer = {};
        // ++depth;
    }

    return errors;
}