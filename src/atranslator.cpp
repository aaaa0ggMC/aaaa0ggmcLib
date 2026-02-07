#include <alib5/atranslator.h>

using namespace alib5;

Translator::Translator(std::pmr::memory_resource* __a)
:translations(__a)
,current_language(__a){
    res = __a;

    /// 内部id名字,比如en_us
    schema["id"] = "REQUIRED TYPE STRING VALIDATE not_empty";
    /// 展示名字,比如 简体中文
    schema["title"] = "REQUIRED TYPE STRING VALIDATE not_empty";

    validator.emplace_validate_method("not_empty",[](AData & node,std::pmr::vector<std::pmr::string> & args){
        if(node.is_value())return !node.to<std::string_view>().empty();
        return false;
    });

    auto msg = validator.from_adata(schema);
    panicf_if(
        msg.size(),
        "CRITICAL_ERROR: compile schema failed! ERR:{}",
        msg
    );
}

bool Translator::load_from_memory(std::string_view data){
    AData tmp;
    /// 目前暂时只支持json
    if(!tmp.load_from_memory(data))return false;
    /// 校验
    vali_result.reset();
    if(!validator.validate(tmp,vali_result)){
        return false;
    }
    /// merge
    if(current_language.empty()){
        current_language = tmp["id"].to<std::string>();
        // std::cout << "SET TO" << current_language << std::endl;
    }
    translations[tmp["id"].to<std::string>()].merge(std::move(tmp));
    return true;
}  

Translator::Result Translator::load_from_entry(io::FileEntry entry,Translator::IsTRFileFn fn){
    Result r;
    if(entry.invalid()){
        r.ecode = err_filesystem_error;
        return r;
    }
    std::pmr::string tmp (res);

    if(entry.is_directory()){
        io::TraverseConfig tr;
        tr.depth = 1;
        tr.keep = { io::FileEntry::regular,io::FileEntry::symlink };
        auto data = io::traverse_files(entry.path,tr);
        std::sort(data.targets_absolute.begin(),data.targets_absolute.end(),[](const io::FileEntry & a,const io::FileEntry & b){
            return a.path < b.path;
        });
        for(auto & file : data.targets_absolute){
            if(fn(file.path)){
                tmp.clear();
                file.read(tmp);
                if(tmp.size() && load_from_memory(tmp)){
                    if(r.enable_success)r.success_files.emplace_back(
                        file.path
                    );
                    r.success_count += 1;
                }else{
                    if(r.enable_failures)r.failure_files.emplace_back(
                        file.path
                    );
                    r.failure_count += 1;
                }
            }
        }
    }else if(fn(entry.path)){
        tmp.clear();
        entry.read(tmp);
        if(tmp.size() && load_from_memory(tmp)){
            if(r.enable_success)r.success_files.emplace_back(
                entry.path
            );
            r.success_count += 1;
        }else{
            if(r.enable_failures)r.failure_files.emplace_back(
                entry.path
            );
            r.failure_count += 1;
        }
    }
    if(!r.success_count)r.ecode = err_none_match;
    return r;
}  

std::string_view Translator::get_key_value(std::string_view key) const {
    // 切割
    // 这里就是纯纯的数据切割,因此不需要拼接字符串
    auto it = translations.object().find(current_language);
    if(it == translations.object().end())
        return key;
    const AData * current = &(*it).second;

    auto vals = str::split(key,'.');
    for(size_t i = 0;i < vals.size();++i){
        if(current->is_array()){
            pvalue_t lite_value = vals[i];
            if(auto v = lite_value.expect<int>()){
                current = current->array().at_ptr(*v);
                if(!current)return key;
            }else return key;
        }else if(current->is_object()){
            std::string_view path = vals[i];
            auto & obj = current->object();
            while(true){
                auto itt = obj.find(path);
                if(itt == obj.end()){
                    ++i;
                    // 连续内存,就不用拼接了
                    if(i < vals.size()){
                        path = std::string_view(path.data(),vals[i].data() + vals[i].length() - path.data());
                        continue;
                    }else return key; // 这个也是没找到
                }
                current = &(*itt).second;
                break;
            }
        }
    }

    if(current->is_value())return current->to<std::string_view>();
    else return key; // 访问的不是根节点
}

std::optional<FlattenTranslator> Translator::flatten_dots(std::string_view key,size_t s,std::pmr::memory_resource * __a){
    if(key.empty())key = current_language;
    if(key.empty())return std::nullopt;
    if(!__a)__a = res;

    auto it = translations.object().find(key);
    if(it == translations.object().end()){
        return std::nullopt;
    }

    // 又是一个DFS?
    FlattenTranslator flat(__a);
    flat.type = FlattenTranslator::Dots;
    flat.key_buffer.reserve(s);
    flat.value_buffer.reserve(s);

    struct Frame{
        AData * node;
        int depth;
        int index {-1};
        std::string_view s {""};
    };
    std::vector<Frame> frames;
    std::vector<size_t> seps;
    std::pmr::string loc (res);
    bool first = true;

    frames.emplace_back(&(*it).second,0);

    auto pop_visi = [&]{
        if(seps.size()){
            loc.resize(seps.back());
            seps.pop_back();
        }
    };

    while(!frames.empty()){
        auto d = frames.back();
        frames.pop_back();
        
        
        if(d.node == nullptr){
            pop_visi();
            continue;
        }
        
        auto push_vis = [&]{
            if(d.index < 0 && d.s.empty())return;
            seps.push_back(loc.size());
            if(d.depth > 1)loc.push_back('.');
            if(d.index >= 0){
                loc += ext::to_string(d.index);
            }else if(!d.s.empty()){
                loc += d.s;
            }
        };

        if(d.node->is_array()){
            push_vis();            
            auto & arr = d.node->array();

            frames.emplace_back(nullptr);
            for(size_t i = 0;i < d.node->array().size();++i){
                frames.emplace_back(&arr[i],d.depth + 1,i);
            }
        }else if(d.node->is_object()){
            push_vis();
            auto & obj = d.node->object();
            frames.emplace_back(nullptr);
            for(auto p : obj){
                frames.emplace_back(&p.second,d.depth + 1,-1,p.first);
            }
        }else if(d.node->is_null())continue;
        else{
            push_vis();
            size_t kb = flat.key_buffer.size();
            flat.key_buffer += loc;
            size_t vb = flat.value_buffer.size();
            flat.value_buffer += d.node->to<std::string_view>();
            
            flat.offsets.emplace_back(std::pair{kb,flat.key_buffer.size()},std::pair{vb,flat.value_buffer.size()});
            pop_visi();
        }
    }
    flat.build_mapper();
    return flat;
}

void FlattenTranslator::build_mapper(){
    mapper.reserve(offsets.size());
    for(auto & d : offsets){
        mapper.emplace(
            std::string_view(key_buffer.data() + d.key.first,d.key.second - d.key.first),
            std::string_view(value_buffer.data() + d.value.first,d.value.second - d.value.first)
        );
    }
}

/// @todo 合并一下呗,代码几乎一模一样来着
std::optional<FlattenTranslator> Translator::flatten_jsonp(std::string_view key,size_t s,std::pmr::memory_resource * __a){
    if(key.empty())key = current_language;
    if(key.empty())return std::nullopt;
    if(!__a)__a = res;

    auto it = translations.object().find(key);
    if(it == translations.object().end()){
        return std::nullopt;
    }

    // 又是一个DFS?
    FlattenTranslator flat(__a);
    flat.type = FlattenTranslator::JsonP;
    flat.key_buffer.reserve(s);
    flat.value_buffer.reserve(s);

    struct Frame{
        AData * node;
        int index {-1};
        std::string_view s {""};
    };
    std::vector<Frame> frames;
    std::vector<size_t> seps;
    std::pmr::string loc (res);
    bool first = true;

    frames.emplace_back(&(*it).second);

    auto pop_visi = [&]{
        if(seps.size()){
            loc.resize(seps.back());
            seps.pop_back();
        }
    };

    while(!frames.empty()){
        auto d = frames.back();
        frames.pop_back();
        
        
        if(d.node == nullptr){
            pop_visi();
            continue;
        }
        
        auto push_vis = [&]{
            if(d.index < 0 && d.s.empty())return;
            seps.push_back(loc.size());
            loc.push_back('/');
            if(d.index >= 0){
                loc += ext::to_string(d.index);
            }else if(!d.s.empty()){
                for(auto ch : d.s){
                    if(ch == '~')loc.append("~0");
                    else if(ch == '/')loc.append("~1");
                    else loc.push_back(ch);
                }
            }
        };

        if(d.node->is_array()){
            push_vis();            
            auto & arr = d.node->array();

            frames.emplace_back(nullptr);
            for(size_t i = 0;i < d.node->array().size();++i){
                frames.emplace_back(&arr[i],i);
            }
        }else if(d.node->is_object()){
            push_vis();
            auto & obj = d.node->object();
            frames.emplace_back(nullptr);
            for(auto p : obj){
                frames.emplace_back(&p.second,-1,p.first);
            }
        }else if(d.node->is_null())continue;
        else{
            push_vis();
            size_t kb = flat.key_buffer.size();
            flat.key_buffer += loc;
            size_t vb = flat.value_buffer.size();
            flat.value_buffer += d.node->to<std::string_view>();
            
            flat.offsets.emplace_back(std::pair{kb,flat.key_buffer.size()},std::pair{vb,flat.value_buffer.size()});
            pop_visi();
        }
    }
    flat.build_mapper();
    return flat;
}