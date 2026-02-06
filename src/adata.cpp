#include <alib5/adata.h>

using namespace alib5;

std::pair<AData*,size_t> AData::Object::ensure_node(std::string_view key){
    auto it = object_mapper.find(key);
    if(it != object_mapper.end()){
        return std::make_pair(&children[it->second],it->second);
    }
    auto alloc = object_mapper.get_allocator();
    // 获取node
    size_t index = 0;
    bool flag;
    AData & node = children.try_next_with_index(flag,index,alloc.resource());
    // 生成mapper
    object_mapper.emplace(std::pmr::string(key,alloc),index);
    return std::make_pair(&node, index);
}

AData::Object::Object(std::pmr::memory_resource* __a)
:children(0,__a,__a) // 目前先不预留空间了
,object_mapper(__a)
{}

AData::Array::Array(std::pmr::memory_resource * __a):values(__a){}

std::span<AData> AData::Array::ensure(size_t size){
    if(size <= values.size())return {};
    auto res = values.get_allocator().resource();
    auto beg = values.size();
    
    values.resize(size,AData(res));

    return std::span(values.begin() + beg,values.end());
}

bool AData::Object::remove(std::string_view name){
    auto it = object_mapper.find(name);
    if(it == object_mapper.end()){
        return false;
    }
    children.remove(it->second);
    object_mapper.erase(it);
    return true;
}

bool AData::Object::rename(std::string_view old_name,std::string_view new_name){
    auto it = object_mapper.find(old_name);
    if(it == object_mapper.end()){
        return false;
    }
    if(old_name == new_name){
        return true;
    }
    auto index = it->second;
    object_mapper.erase(it);
    object_mapper.emplace(
        std::pmr::string(new_name,object_mapper.get_allocator()),
        index
    );
    return true;
}

const AData * AData::jump_ptr(std::string_view path,bool err) const{
    if(path.empty())return this;
    if(path[0] != '/'){
        if(err)invoke_error(err_locate_error,"Failed to parse pointer which isn't begin with '/'!PATH:{}",path);
        return nullptr;
    }
    auto splits = str::split(path,'/');
    std::span<std::string_view> main = splits;
    // 一定可以切割出两个了
    main = main.subspan(1);

    const AData * current = this;
    int index = 0;
    Value val;

    while(index < main.size()){
        std::string_view s = main[index];
        ++index;
        
        val = s;

        if(auto ss = val.expect<int>(); ss.second && current->is_array()){
            auto * ptr = current->array().at_ptr(ss.first);
            if(ptr) current = ptr;
            else{
                if(err){
                    invoke_error(err_locate_error,"Locate failed when finding array index {}!",ss.first);
                }
                return nullptr;
            }
        }else if(current->is_object()){
            // 这里要进行一波转换
            static thread_local std::string cast = "";
            cast.clear();
            for(size_t i = 0;i < s.size();++i){
                if(s[i] == '~' && i+1 < s.size()){
                    if(s[i + 1] == '0'){
                        cast.push_back('~');
                        ++i;
                        continue;
                    }else if(s[i + 1] == '1'){
                        cast.push_back('/');
                        ++i;
                        continue;
                    }
                }
                cast.push_back(s[i]);
            }
            auto *ptr = current->object().at_ptr(cast);
            if(ptr) current = ptr;
            else{
                if(err){
                    invoke_error(err_locate_error,"Locate failed when finding object name {}!",cast);
                }
                return nullptr;
            }
        }else{
            if(err){
                invoke_error(err_locate_error,"Locate failed for the lack of depth!");
            }
            return nullptr;
        }
    }
    return current;
}