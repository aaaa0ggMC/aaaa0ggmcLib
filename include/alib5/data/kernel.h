#ifndef ALIB5_ADATA_KERNEL
#define ALIB5_ADATA_KERNEL
#include <alib5/autil.h>
#include <alib5/adebug.h>
#include <alib5/aref.h>
#include <alib5/ecs/linear_storage.h>

namespace alib5{
    /// 数组访问自动扩展
    static constexpr size_t conf_array_auto_expand = 4;

    template<class T> concept IsNodeValue = 
        std::is_integral_v<std::decay_t<T>> || std::is_floating_point_v<std::decay_t<T>> ||
        std::is_same_v<std::decay_t<T>,bool> || IsStringLike<std::decay_t<T>> || IsStringLike<T>;

    template<class T,class Data> concept IsDataPolicy = requires(T & t,std::pmr::string & dmp,std::string_view data,Data & d){
        t.parse(data,d);
        t.dump(dmp,d); // 至少要求dump到pmr::string
    }; // 不打算阻止这个了 && !std::is_void_v<decltype(std::declval<T>().parse(std::declval<std::string_view>(), std::declval<Data&>()))>;

    /// 默认值
    namespace data{
        class JSON;
    };

    /// 数值
    /// @warning 多线程下大概率不能正常工作,因此多线程下必须加锁!!!
    struct ALIB5_API Value{            
        enum Type{
            STRING,
            INT,
            FLOATING,
            BOOL
        };
    private:
        /// 并不能保证线程安全,因此这一块还需要考量一下
        mutable std::pmr::string data;
        mutable bool data_dirt;

        Type type;
        /// 普通数据存储区域
        union{
            int64_t integer;
            double floating;
            bool boolean;
        };

        /// 脏标记
        inline void mark() const { data_dirt = true; }

        /// 同步数据
        inline void back_sync(std::string_view d) const {
            [[likely]] if(data_dirt){
                data_dirt = false;
                this->data = d;
            }
        }

        /// 内部数据的同步
        void sync_to_string() const;
    public:
        /// 需要实现修改逻辑
        /// 需要确保使用短期引用
        /// 这里表示是否上报error
        template<class T,bool invoke_err = true> auto& transform();

        /// 这里强行重建变量
        /// 注意数据没有被清除,因此内部保存有垃圾值
        template<class T> auto& reconstruct();

        /// 数据可能存在不同不
        std::string_view raw_view() const { return data; }

        /// 获取当前的类型
        inline Type get_type() const { return type; }

        /// 初始化数据
        Value(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE):data(__a){
            type = STRING;
            data_dirt = false;
            integer = 0;
        }
        /// 初始化数据-通用版本
        template<class T> requires(!IsStringLike<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, Value>)
        Value(const T && d,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE):data(__a){
            // 无非只是多计算一次
            type = STRING;
            data_dirt = true;
            transform<std::decay_t<T>>() = std::forward<T>(d);
        }
        /// 初始化数据-字符串版本特化
        template<IsStringLike STR>
        Value(STR && d,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :data(std::forward<STR>(d),__a){
            type = STRING;
            data_dirt = false;
            integer = 0;
        }

        /// 设置数值
        template<class T> Value& set(T && val);
        /// 赋值
        template<IsNodeValue T> inline Value& operator=(T && val){ set(std::forward<T>(val)); return *this; }
        /// 校验类型,不更改内部类型,返回值
        /// pair<转换结果,转换是否成功>
        template<class T> auto expect() const;
        /// 转换类型,不更改内部类型
        template<class T> auto to() const;
    };

    /// 节点
    struct ALIB5_API AData{
        /// 对象
        struct ALIB5_API Object{
            using container_t = ecs::detail::LinearStorage<AData>;
            /// 子类
            container_t children;
            /// 映射
            std::pmr::unordered_map<
                std::pmr::string,
                size_t,
                detail::TransparentStringHash,
                detail::TransparentStringEqual
            > object_mapper;
        
            /// 构造对象
            ALIB5_API Object(std::pmr::memory_resource* __a);
            /// Object对索引对齐要求十分严格,因此多线程操作一定要自己加锁
            /// 实际上多线程操作数据加锁应该是常识
            std::pair<AData*,size_t> ALIB5_API ensure_node(std::string_view key);
            /// 改名字
            bool ALIB5_API rename(std::string_view old_name,std::string_view new_name);
            /// 删除对象
            bool ALIB5_API remove(std::string_view name);

            /// 注意返回的不是Node&,因此operator[]不是node的operator[]
            inline RefWrapper<container_t> safe_visit(std::string_view visit){ return ref(children,ensure_node(visit).second); }

            /// 引用可能失效
            inline AData& operator[](std::string_view visit){ return *ensure_node(visit).first; }
        
            /// at_ptr
            inline const AData* at_ptr(std::string_view visit) const{
                auto it = object_mapper.find(visit);
                if(it == object_mapper.end())return nullptr;
                return &children.data[it->second];
            }
            inline AData* at_ptr(std::string_view visit){
                return const_cast<AData *>(
                    (((const AData::Object *)(this))->at_ptr(visit))
                );
            }

            /// 清空数据
            inline void clear(){
                children.clear();
                object_mapper.clear();
            }

            /// 并不会创建新的对象,因此会在没有对象的时候崩溃
            inline const AData& operator[](std::string_view visit) const{
                auto a = at_ptr(visit);
                panicf_if(!a,"Invalid visit {}!",visit);
                return *a;
            }

            inline size_t size() const { 
                return object_mapper.size();
            }
            inline bool empty() const {
                return !object_mapper.size();
            }
            inline bool contains(std::string_view key) const {
                return object_mapper.find(key) != object_mapper.end();
            }

            template<bool is_const>
            struct ObjectIterator{
                using iterator_category = std::forward_iterator_tag;
                using value_type = AData;
                using difference_type = std::ptrdiff_t;
                using pointer = std::conditional_t<is_const, const AData*, AData*>;
                using reference = std::conditional_t<is_const, const AData&, AData&>;
                
                std::conditional_t<is_const,
                    decltype(object_mapper)::const_iterator,
                    decltype(object_mapper)::iterator
                > it;
                std::conditional_t<is_const,
                    const container_t&,
                    container_t&    
                > cont;

                struct Proxy{
                    const std::pmr::string & first;
                    reference second;

                    auto* operator->(){return &second;}
                };


                ObjectIterator(decltype(it) iter,decltype(cont) container)
                :it(iter)
                ,cont(container)
                {}



                auto operator*(){
                    return Proxy{it->first,cont.data[it->second]};
                }
                auto operator++(){ return ++it; }
                auto operator++(int){ return it++; }
                bool operator==(const ObjectIterator& other) const { return it == other.it; }
                bool operator!=(const ObjectIterator& other) const { return it != other.it; }
                auto operator->(){ return Proxy{it->first,cont.data[it->second]}; } // 这里也符合end()->会panic的性质
            };

            using iterator = ObjectIterator<false>;
            using const_iterator = ObjectIterator<true>;
            using value_type = decltype(children.data)::value_type;

            iterator begin(){ return {object_mapper.begin(),children};}
            const_iterator begin() const { return {object_mapper.begin(),children};}
            iterator end(){ return {object_mapper.end(),children};}
            const_iterator end() const { return {object_mapper.end(),children};}
        
            iterator find(std::string_view d){ return {object_mapper.find(d),children}; }
            const_iterator find(std::string_view d) const { return {object_mapper.find(d),children}; }
        };

        /// Array
        struct ALIB5_API Array{
            using container_t = std::pmr::vector<AData>;
            /// 子类
            container_t values;

            ALIB5_API Array(std::pmr::memory_resource * __a);

            /// 比较安全的访问
            /// 但是禁不起shrink
            RefWrapper<container_t> safe_visit(std::ptrdiff_t index);

            /// 引用可能失效
            AData& operator[](std::ptrdiff_t index);

            /// const化
            const AData& operator[](std::ptrdiff_t index) const;

            /// 保证数量,返回新建立的元素的span
            std::span<AData> ALIB5_API ensure(size_t size);

            /// 获取大小
            inline size_t size() const { return values.size(); }
            /// 清空?
            inline void clear(){ values.clear(); }
            /// empty?
            inline bool empty() const { return values.empty(); }
            //// 剩下的操作用户自己操控values吧 //// 

            /// 提供at访问
            inline const AData* at_ptr(ptrdiff_t index) const{
                if(index < 0)index = values.size() + index;
                if(index >= values.size())return nullptr;
                return &values[index];
            } 
            inline AData* at_ptr(ptrdiff_t index) {
                return const_cast<AData*>(
                    (((const Array *)(this))->at_ptr(index))
                );
            }

            auto begin(){ return values.begin(); }
            auto begin() const { return values.begin(); }
            auto end(){ return values.end(); }
            auto end() const { return values.end(); }
        };

        /// 类型标记,和variant对齐
        enum Type{
            TNull,
            TValue,
            TObject,
            TArray
        };

    private:
        /// 值
        std::variant<std::monostate,Value,Object,Array> data;
        /// 分配器
        std::pmr::memory_resource * allocator;
        
        /// 返回对象
        template<class T> auto& __get_value() const {
            using type = std::decay_t<T>;
            const type * f = std::get_if<type>(&data);
            [[likely]] if(f){
                return *f;
            }
            // 这个比较严重,而且底层,因此还是panic好一点
            vpanicf_if(!f,
                "Type doesnt match!CurrentId:{}(0 null,1 value,2 object,3 array),RequestedType:{}",
                (int)get_type(),typeid(type).name());
            // 理论上不会被执行,执行了也是panic
            std::abort();
        }

        template<class T> inline auto& __get_value() {
            return const_cast<std::decay_t<T>&>(
                static_cast<const AData*>(this)->__get_value<T>()
            );
        }

        template<class T> auto& __ensure_type(){
            using type = std::decay_t<T>;
            if(std::holds_alternative<std::monostate>(data)){
                return set<T>();
            }else [[likely]] {
                return __get_value<T>();
            }
        }

        static auto clone_data(const decltype(data)& src, std::pmr::memory_resource* res) {
            return std::visit([res](auto&& v) -> decltype(data) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) return v;
                else if constexpr (std::is_same_v<T, Value>){
                    Value x(res);
                    x = v;
                    return x; 
                }else if constexpr (std::is_same_v<T, Object>){
                    Object o(res);
                    o = v;
                    return o;
                }else{
                    Array a(res);
                    a = v;
                    return a;
                }
            }, src);
        }
    public:
        inline std::pmr::memory_resource* get_allocator(){ return allocator; }

        /// 构造data
        template<class T> T& set();
        
        /// 这里提供重新设置的方案
        auto& set_null(){ return set<std::monostate>(); }

        /// 基础构造
        explicit AData(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE){
            allocator = __a;
            // set<std::monostate>();
        }
        template<IsNodeValue T> 
        requires (!std::is_same_v<std::remove_cvref_t<T>, AData>)
        AData(T && val,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE){
            allocator = __a;
            this->operator=(std::forward<T>(val));   
        }
        AData(const AData& other) : allocator(other.allocator){
            data = other.data;
        }
        AData(AData&& other) noexcept : allocator(other.allocator){
            *this = std::move(other);
        }
        AData(const AData& other, std::pmr::memory_resource* __a):allocator(__a){
            data = clone_data(other.data, __a);
        }
        ~AData() = default;

        /// 解决父子之间赋值出现的crash
        /// 问题已经被处理了,调整了赋值链条,但是api保留一下
        AData detach() const{
            // 相当于是复制一份数据
            return *this;
        }

        /// 类型判断
        inline Type get_type() const{ return (Type)data.index(); }
        inline bool is_null() const { return get_type() == TNull; }
        inline bool is_object() const { return get_type() == TObject; }
        inline bool is_array() const { return get_type() == TArray; }
        inline bool is_value() const { return get_type() == TValue; }

        Value & value(){return __get_value<Value>();}
        Object & object(){return __get_value<Object>();}
        Array & array(){return __get_value<Array>();}
        std::monostate& null(){return __get_value<std::monostate>();}
        const Value & value() const {return __get_value<Value>();}
        const Object & object() const {return __get_value<Object>();}
        const Array & array() const {return __get_value<Array>();}
        const std::monostate& null() const {return __get_value<std::monostate>();}

        template<IsNodeValue T> AData& operator=(T && val){
            Value & v = __ensure_type<Value>();
            v = std::forward<T>(val);
            return *this;
        }

        template<class T> AData& rewrite(T && val){
            // 置空
            set<std::monostate>();
            *this = std::forward<T>(val);
            return *this;
        }

        /// 对于move对象自动重置为NULL
        AData& rewrite(AData&& other) {
            using safe_t = std::remove_cv_t<decltype(data)>;
            if(this == &other)return *this;
            safe_t d = std::move(other.data);
            // 防止other引用失效
            other.set<std::monostate>();

            if(this->allocator == other.allocator){
                this->data = std::move(d);
            }else{
                // this->set<std::monostate>();
                this->data = clone_data(d, allocator);
            }
            return *this;
        }

        /// 对于右值,std::move强调了数据的转移,因此进行覆盖
        /// move对象类型重置为NULL
        AData& operator=(AData && val){
            rewrite(std::move(val));    
            return *this;
        }

        AData& operator=(const AData & val){
            [[unlikely]] if(this == &val)return *this;
            [[unlikely]] if(get_type() != TNull && val.get_type() != TNull && get_type() != val.get_type()){
                // 这里再一次判断是为了把这个判断信息写入输出
                panic_if(get_type() != val.get_type(),"Implicit type cast is forbidden.");
                std::abort();
            }
            
            if(val.allocator == this->allocator){
                /// 复制版本
                decltype(data) safe_d = val.data;
                data = std::move(safe_d);
            }else{
                this->data = clone_data(val.data, allocator);
            }
            return *this;
        }

        AData& operator[](std::string_view visit){
            Object & o = __ensure_type<Object>();
            return o[visit];
        }

        const AData& operator[](std::string_view visit) const {
            return object()[visit];
        }

        AData& operator[](std::ptrdiff_t visit){
            Array & o = __ensure_type<Array>();
            return o[visit];
        }

        /// 转换,需要是value,不然报错
        template<class T> auto to() const {
            return value().to<T>();
        }
        template<class T> auto try_to() -> std::optional<decltype(to<T>())> const {
            if(get_type() != TValue)return std::nullopt;
            return std::optional(to<T>());
        }
        /// 判断,需要是value,不然报错
        template<class T> auto expect() const {
            return value().expect<T>();
        }
        template<class T> auto try_expect() -> std::optional<decltype(expect<T>())> const {
            if(get_type() != TValue)return std::nullopt;
            return std::optional(expect<T>());
        }

        //// 主要区域-1 : 读取数据,直接覆盖当前的类型 ////
        /// 从内存中读取
        template<IsDataPolicy<AData> DataPolicy = data::JSON> auto load_from_memory(std::string_view data,DataPolicy && parser = DataPolicy()){
            return std::forward<DataPolicy>(parser).parse(data,*this);
        }
        /// 从文件中读取
        template<IsDataPolicy<AData> DataPolicy = data::JSON> auto load_from_entry(io::FileEntry entry,DataPolicy && parser = DataPolicy()){
            panic_debug(entry.invalid(),"Entry is invalid!");
            // release模式下invalid read出来是""
            return load_from_memory(entry.read(),std::forward<DataPolicy>(parser));
        }
        template<IsDataPolicy<AData> DataPolicy = data::JSON> auto load_from_file(std::string_view path,DataPolicy && parser = DataPolicy()){
            return load_from_entry(io::load_entry(path),std::forward<DataPolicy>(parser));
        }

        /// 写入对象
        template<IsDataPolicy<AData> Dumper = data::JSON,class T> auto dump(T & target,Dumper && dumper = Dumper()){
            return std::forward<Dumper>(dumper).dump(target,*this);
        }
        /// 写入pmr::string
        template<IsDataPolicy<AData> Dumper = data::JSON> auto dump_to_string(Dumper && dumper = Dumper(),std::pmr::memory_resource * res = ALIB5_DEFAULT_MEMORY_RESOURCE){
            std::pmr::string str (res);
            dump(str,std::forward<Dumper>(dumper));
            return str;
        }
        /// 写入entry
        template<IsDataPolicy<AData> Dumper = data::JSON> auto dump_to_entry(const io::FileEntry& entry,Dumper && dumper = Dumper(),std::pmr::memory_resource * res = ALIB5_DEFAULT_MEMORY_RESOURCE){
            std::pmr::string str (res);
            auto val = dump(str,std::forward<Dumper>(dumper));
            entry.write_all(str);
            return val;
        }       
        /// 写入文件
        template<IsDataPolicy<AData> Dumper = data::JSON> auto dump_to_file(std::string_view file_path,Dumper && dumper = Dumper(),std::pmr::memory_resource * res = ALIB5_DEFAULT_MEMORY_RESOURCE){
            /// 不在意文件是否存在
            return dump_to_entry(io::load_entry(file_path,false),std::forward<Dumper>(dumper),res);
        }
    };

    using dvalue_t = Value;
    using dadata_t = AData;
    using dobject_t = AData::Object;
    using darray_t = AData::Array;
    using dtype_t = AData::Type;
    using dvalue_type_t = Value::Type;
}

namespace alib5{
    inline void Value::sync_to_string() const {
        if(!data_dirt) return;
        switch(type) {
            case INT:      data = ext::to_string(integer); break;
            case FLOATING: data = ext::to_string(floating); break;
            case BOOL:     data = ext::to_string(boolean); break;
            default: break;
        }
        data_dirt = false;
    }

    template<class T> inline auto& Value::reconstruct(){
        if constexpr(IsStringLike<T>){
            type = STRING;
            data_dirt = false;
            return data;
        }else{
            data_dirt = true; 
            
            if constexpr(std::is_same_v<T, bool>){
                type = BOOL;
                return boolean;
            }else if constexpr(std::is_integral_v<T>){
                type = INT;
                return integer;
            }else if constexpr(std::is_floating_point_v<T>){
                type = FLOATING;
                return floating;
            }
        }
    }

    template<class T,bool invoke_err> inline auto& Value::transform(){
        auto generator = [this]<class T1,class T2,class T3,Type t1,Type t2,Type t3>(T1 & v1,T2 & v2,T3 & v3) -> auto& {
            [[likely]] if(type == t1){
            }else if(type == t2){
                v1 = (T1)v2;
            }else if(type == t3){
                v1 = (T1)v3;
            }else if(type == STRING){
                // 如果不拿result接取to_T也会报错,因此这里还是要接取
                std::from_chars_result result {};
                v1 = ext::to_T<T1>(str::trim(data),&result);
                if constexpr(invoke_err){
                    if(result.ec != std::errc()){
                        invoke_error(err_format_error,"Failed to format \"{}\".",data);
                    }
                }
            }
            type = t1;
            data_dirt = true;
            return v1;
        };
        
        if constexpr(IsStringLike<T>) {
            sync_to_string();
            type = STRING;
            // 字符串短期修改始终最新
            return data;
        }else if constexpr(std::is_same_v<T,bool>){
            return generator.template operator()<bool,double,int64_t,BOOL,FLOATING,INT>(
                boolean,floating,integer
            );
        }else if constexpr(std::is_integral_v<T>){
            return generator.template operator()<int64_t,double,bool,INT,FLOATING,BOOL>(
                integer,floating,boolean
            );
        }else if constexpr(std::is_floating_point_v<T>){
            return generator.template operator()<double,int64_t,bool,FLOATING,INT,BOOL>(
                floating,integer,boolean
            );
        }else {
            static_assert(std::is_same_v<T,bool>,"Unsupported type!");
        }
    }

    template<class T> inline auto Value::expect() const {
        auto make_value = [this]<class T1>(T1 & val){
            if constexpr(IsStringLike<T>){
                if(data_dirt)back_sync(ext::to_string(val));
                return std::make_pair(std::string_view(data),true);
            }else return std::make_pair(T(val),true);
        };

        switch(type){
        case STRING:{
            std::from_chars_result result {};
            auto v = ext::to_T<T>(data,&result);
            return std::make_pair(v,result.ec == std::errc() && result.ptr == (data.data() + data.size()));
        }
        case INT:
            return make_value(integer);
        case FLOATING:
            return make_value(floating);
        default: // BOOL
            return make_value(boolean);
        }
    }

    template<class T> inline auto Value::to() const{
        auto ret = expect<T>();
        if(!ret.second){
            invoke_error(err_format_error,"Cannot format \"{}\" correctly!",data);
        }
        return ret.first;
    }

    template<class T> inline Value& Value::set(T && val){
        // 这里自动设置dirty了
        auto & ref = reconstruct<std::decay_t<T>>();
        ref = std::forward<T>(val);
        return *this;
    }

    inline RefWrapper<AData::Array::container_t> AData::Array::safe_visit(std::ptrdiff_t index){
        if(index < 0)index = values.size() + index;
        [[unlikely]] panic_if(index >= values.size(),"Array out of bounds!");
        return ref(values,index);
    }

    inline AData& AData::Array::operator[](std::ptrdiff_t index){
        if(index < 0)index = values.size() + index;
        [[unlikely]] panic_if(index < 0 || index >= values.size() + conf_array_auto_expand,"Array out of bounds!");
        if(index >= values.size())values.resize(index + 1,AData(values.get_allocator().resource()));
        return values[index];
    }

    inline const AData& AData::Array::operator[](std::ptrdiff_t index) const{
        if(index < 0)index = values.size() + index;
        [[unlikely]] panic_if(index < 0 || index >= values.size(),"Array out of bounds!");
        return values[index];
    }

    template<class T> inline T& AData::set(){
        if constexpr(std::is_same_v<T,std::monostate>) {
            return data.emplace<std::monostate>();
        }else return data.emplace<T>(allocator);
    }
}

#endif