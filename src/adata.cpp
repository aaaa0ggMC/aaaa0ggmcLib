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


bool Value::equals(const Value & left,const Value & right,CompareStrategy st){
    auto float_equal = [](double a, double b){
        if (a == b) return true;
        double diff = std::abs(a - b);
        if(diff < conf_value_compare_epsilon)return true;
        
        return diff <= ( (std::abs(a) < std::abs(b) ? std::abs(b) : std::abs(a)) 
                        * std::numeric_limits<double>::epsilon() );
    };
    
    auto lt = left.get_type();
    auto rt = right.get_type();
    if(lt == rt){
        switch(lt){
        case Type::BOOL:
            return left.boolean == right.boolean;
        case Type::FLOATING:
            if(st == CompareStrategy::Strict){
                return left.floating == right.floating; 
            }
            // 只有非Strict模式才允许Epsilon容错
            return float_equal(left.floating, right.floating);
        case Type::INT:
            return left.INT == right.INT;
        case Type::STRING:
            return left.data == right.data;
        }
        panic_debug(true,"aaaa0ggmc,你这里忘记处理其他类型了!!!同时注意下面的代码也要同步你的类型更改");
    }
    if(st == CompareStrategy::Strict)return false;
    // 现在是Lesser : 可以比较INT和DOUBLE啥的
    auto is_numeric = [](Type t){
        return t == Type::FLOATING || t == Type::INT || t == Type::BOOL;
    };
    // 在这里left type != right type绝对成立
    if(is_numeric(lt) && is_numeric(rt)){
        // 在这里,二者都不是string
        if(lt == Type::BOOL || rt == Type::BOOL){
            if(st == CompareStrategy::BoolStrict){
                return float_equal(left.to<double>(),right.to<double>());
            }else {
                return left.to<bool>() == right.to<bool>();
            }
        }
        // 这样下面就没有bool干扰了
        // 所以目前只有DOUBLE和INT了
        /// @note 这里需要同步更改
        // 一定有一方是INT,一方是DOUBLE
        return right.to<double>() == left.to<double>();
    }
    if(st == CompareStrategy::BoolStrict || st == CompareStrategy::Lesser)return false;
    // 一定存在一方是string
    auto& plt = (lt == Type::STRING)? right : left;
    auto& prt = (lt == Type::STRING)? left : right;
    lt = plt.get_type();
    rt = prt.get_type();
    
    if(lt == Type::FLOATING){
        // rt为STRING
        auto db = prt.expect<double>();
        // 转换失败
        if(!db.second)return false;
        return float_equal(db.first,plt.floating);
    }else if(lt == Type::INT){
        auto db = prt.expect<double>();
        // 转换失败
        if(!db.second)return false;
        return float_equal(db.first,(double)plt.integer);
    }else if(lt == Type::BOOL){
        std::string_view s = prt.data;
        // std::cout << "PRT:" << s << std::endl;
        if(plt.boolean == true){
            if(s == "true" || s == "TRUE" || s == "1" || s == "yes" || s == "YES"){
                return true;
            }
            return false;
        }else{
            if(s == "false" || s == "FALSE" || s == "0" || s == "no" || s == "NO"){
                return true;
            }   
            return false;
        }
    }
    return false;
}

bool AData::equals(const AData & left,const AData & right,CompareStrategy st){
    struct Frame{
        const AData* left;
        const AData* right;
    };
    std::deque<Frame> frames;
    frames.emplace_back(&left,&right);

    while(!frames.empty()){
        Frame f = frames.back();
        frames.pop_back();

        // 大类型都不相等,别想了
        auto lt = f.left->get_type();
        auto rt = f.right->get_type();
        if(lt != rt){
            return false;
        }
        switch(lt){
        case Type::TValue:
            // 转发数值
            if(!f.left->value().equals(f.right->value(),st))return false;
            break;
        case Type::TArray:{
            auto & la = f.left->array();
            auto & ra = f.right->array();

            // 数组长度
            if(la.size() != ra.size())return false;
            // 对于每个数据元素实施便利
            for(size_t i = 0;i < la.size();++i){
                frames.emplace_back(&la[i],&ra[i]);
            }
            break;
        }
        case Type::TObject:{
            auto & lo = f.left->object();
            auto & ro = f.right->object();

            if(lo.size() != ro.size())return false;
            // 每个成员进行遍历
            for(auto it : lo){
                auto proxy = ro.find(it.first());
                if(proxy == ro.end()){
                    return false;
                }
                // 加入新的
                frames.emplace_back(&it.second(),&proxy.second());
            }
            break;
        }
        case Type::TNull:
            break;
        }
    }
    return true;
}