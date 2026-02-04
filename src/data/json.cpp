#include <alib5/adata.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>

using namespace alib5;
using namespace alib5::data;

struct ADataHandler{
    std::vector<AData*> stack;
    std::string last_key;

    ADataHandler(AData& root){
        stack.push_back(&root);
    }

    AData& current(){ return *stack.back(); }

    AData& prepare_node() {
        AData& c = current();
        if(c.get_type() == AData::TObject){
            return c[last_key];
        }else if(c.get_type() == AData::TArray){
            return c.array().values.emplace_back(c.get_allocator());
        }
        return c;
    }

    // --- RapidJSON Events ---
    bool Null() { prepare_node().set<std::monostate>(); return true; }
    bool Bool(bool b){ prepare_node() = b; return true; }
    bool Int(int i){ prepare_node() = (int64_t)i; return true; }
    bool Uint(unsigned u){ prepare_node() = (int64_t)u; return true; }
    bool Int64(int64_t i){ prepare_node() = i; return true; }
    bool Uint64(uint64_t u){ prepare_node() = (int64_t)u; return true; }
    bool Double(double d){ prepare_node() = d; return true; }
    
    bool String(const char* str, rapidjson::SizeType length, bool copy){
        prepare_node() = std::string_view(str, length);
        return true;
    }

    bool StartObject(){
        AData& node = prepare_node();
        node.set<AData::Object>();
        stack.push_back(&node);
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool copy){
        last_key = std::string_view(str, length);
        return true;
    }

    bool EndObject(rapidjson::SizeType memberCount) {
        stack.pop_back();
        return true;
    }

    bool StartArray() {
        AData& node = prepare_node();
        node.set<AData::Array>();
        stack.push_back(&node);
        return true;
    }

    bool EndArray(rapidjson::SizeType elementCount) {
        stack.pop_back();
        return true;
    }

    // 原始内容交给rapidjson自己处理
    bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) {
        return false; 
    }
};

bool JSON::parse(std::string_view v,dadata_t & root){
    root.set<std::monostate>();
    using namespace rapidjson;
    Reader reader;
    MemoryStream ss(v.data(), v.size());
    ADataHandler handler(root);

    ParseResult res = reader.Parse(ss, handler);
    
    return (bool)res;
}

bool JSON::__internal_dump(__dump_fn fn,void * p,dadata_t & root){
    struct Frame{
        dadata_t* data;
        int depth;
        std::optional<std::string_view> name;
        int index;
        bool last_child;
        enum Action{
            PROCESS,
            WRITE_CLOSE_OBJ,
            WRITE_CLOSE_ARR
        };
        Action action {PROCESS};
    };
    
    std::vector<Frame> queue;
    /// SSO
    std::string indents = "";

    std::string_view place_comma = cfg.compact_lines ? "," : ",\n";
    std::string_view place_next = cfg.compact_lines ? (cfg.compact_spaces?"":" ") : "\n";
    std::string_view arr_new = "";
    std::string_view obj_new = "";

    if(cfg.compact_lines){
        if(cfg.compact_spaces){
            arr_new = "[";
            obj_new = "{";
        }else{
            arr_new = "[ ";
            obj_new = "{ ";
        }
    }else{
        arr_new = "[\n";
        obj_new = "{\n";
    }

    auto get_indent = [this,&indents](int indent) -> std::string_view{
        if(indent < 0)indent = 0;
        if(indents.size() < indent){
            indents.resize(indent,cfg.dump_indent_char);
        }
        return std::string_view(indents.begin(),indents.begin() + indent);
    };
    
    queue.push_back({
        &root,
        0,
        std::nullopt,
        -1,
        true
    });

    auto place = [&](Frame & current){
        if(!current.last_child){
            fn(place_comma, p);
        }else{
            fn(place_next, p);
        }
    };

    struct ObjFrame {
        std::string_view name;
        AData * node;
    };
    std::vector<ObjFrame> frames;

    while(!queue.empty()){
        Frame current = queue.back();
        queue.pop_back();

        if(current.action == Frame::WRITE_CLOSE_OBJ){
            if(!cfg.compact_lines)fn(get_indent(current.depth * cfg.dump_indent),p);
            
            fn("}",p);
            place(current);
            continue;
        }else if(current.action == Frame::WRITE_CLOSE_ARR){
            if(!cfg.compact_lines)fn(get_indent(current.depth * cfg.dump_indent),p);
            fn("]",p);
            place(current);
            continue;
        }

        // 先到达indent
        if(!cfg.compact_lines)fn(get_indent(current.depth * cfg.dump_indent),p);
        if(current.name){
            // object 处理逻辑
            fn("\"",p);
            fn(*current.name,p);
            if(cfg.compact_spaces) fn("\":",p);
            else fn("\" : ",p);
        }
        if(current.data->is_null()){
            fn("null",p);
            place(current);
        }else if(current.data->is_value()){
            auto & v = current.data->value();
            if(v.get_type() != v.STRING){
                fn(v.to<std::string>(),p);
            }else{
                fn("\"",p);
                fn(str::escape(v.to<std::string>()),p);
                fn("\"",p);
            }
            place(current);
        }else if(current.data->is_array()){
            int index = 0;
            size_t sz = current.data->array().size();
            fn(arr_new,p);
            queue.push_back({
                nullptr,
                current.depth,
                std::nullopt,
                -1,
                current.last_child,
                Frame::WRITE_CLOSE_ARR
            });
            auto & arr = current.data->array();
            for(size_t i = arr.size();i > 0;--i){
                queue.push_back({
                    &arr[i-1],
                    current.depth + 1,
                    std::nullopt,
                    (int)(i-1),
                    (i == sz),
                    Frame::PROCESS
                });
            }
        }else if(current.data->is_object()){
            size_t sz = current.data->object().size();
            int index = sz;
            fn(obj_new,p);
            queue.push_back({
                nullptr,
                current.depth,
                std::nullopt,
                -1,
                current.last_child,
                Frame::WRITE_CLOSE_OBJ
            });
            if(cfg.sort_object){
                frames.clear();
                for(auto proxy : current.data->object()){
                    /// 过滤节点
                    if(cfg.filter && !cfg.filter(proxy.first,proxy.second))
                        continue;
                    frames.emplace_back(ObjFrame{
                        proxy.first,
                        &proxy.second
                    });
                }
                std::sort(frames.begin(),frames.end(),[this](const ObjFrame & a,const ObjFrame & b){
                    return !cfg.sort_object(a.name,b.name);
                });
                /// 倒着插入
                // 这里把filter影响除去
                index = sz = frames.size();
                for(auto proxy : frames){
                    queue.push_back({
                        proxy.node,
                        current.depth + 1,
                        proxy.name,
                        -1,
                        (index == sz)
                    });
                    --index;
                }
            }else{
                for(auto proxy : current.data->object()){
                    /// 过滤节点,这个地方是一定会变化的,因此不用管
                    /// 所以它并不是最后一个
                    if(cfg.filter && !cfg.filter(proxy.first,proxy.second)){
                        continue;
                    }
                    queue.push_back({
                        &proxy.second,
                        current.depth + 1,
                        proxy.first,
                        -1,
                        (index == sz)
                    });
                    --index;
                }
            }
        }
    }
    return true;
}