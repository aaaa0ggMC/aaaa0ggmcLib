#ifndef ALIB5_ADATA_VALIDATOR
#define ALIB5_ADATA_VALIDATOR
#include <alib5/data/kernel.h>
#include <alib5/aparser.h>

namespace alib5{

    /// 校验器
    struct ALIB5_API Validator{
        struct ALIB5_API Node{
            enum TypeRestrict{
                RNone,
                RNull,
                RValue, // 其实优先级大于string int double bool
                RString,
                RInt,
                RDouble,
                RBool,
                RArray,
                RObject
            };
            /// 是否必要,如果必要,则要么报错要么默认值构造,如果不必要,那么不会进行默认值填充
            bool required;
            TypeRestrict type_restrict;
            /// 对于ARRAY,限制长度,对于Object,限制成员数量
            /// <0表示没有要求
            dvalue_t min_length;
            dvalue_t max_length;
            /// 校验器,依次排列,其实校验器也能修改节点,string_view指向activated_pool
            struct Validate{
                std::string_view method;
                std::pmr::vector<std::pmr::string> args;

                Validate(std::pmr::memory_resource * __a):args(__a){}
                Validate(const Validate& other, std::pmr::memory_resource* __a)
                :method(other.method),args(other.args, __a){}

                Validate(const Validate&) = default;
                Validate(Validate&&) ALIB5_NOEXCEPT = default;
                Validate& operator=(const Validate&) = default;
                Validate& operator=(Validate&&) ALIB5_NOEXCEPT = default;
            };
            std::pmr::vector<Validate> validates;
            /// 对于类型,默认值就是默认值
            /// 对于数组,默认值是子的默认值
            std::optional<AData> default_value;

            // validate对性能要求没那么高,选择更加方便的存储方式
            /// 校验子节点,仅作用于Object
            std::pmr::unordered_map<std::pmr::string, Node> children;
            /// 类型为ARRAY,那么校验的是这个
            std::pmr::vector<Node> array_subs;
            /// 因为ARRAY分tuple和list,这个作为标识
            bool is_tuple;
            /// 如果存在默认值,那么冲突的时候覆盖
            bool override_if_conflict;
            
            Node(std::pmr::memory_resource * __a = std::pmr::get_default_resource())
            :validates(__a),min_length(__a),max_length(__a),children(__a),array_subs(__a){
                reset();
            }

            Node(const Node& __other, std::pmr::memory_resource* __a)
            :required(__other.required)
            ,type_restrict(__other.type_restrict)
            ,min_length(__other.min_length, __a)
            ,max_length(__other.max_length, __a)
            ,validates(__a) // 这里先初始化空 vector
            ,default_value(std::nullopt)
            ,children(__other.children, __a)
            ,array_subs(__other.array_subs, __a)
            ,is_tuple(__other.is_tuple)
            ,override_if_conflict(__other.override_if_conflict){
                validates.reserve(__other.validates.size());
                for(auto & __v : __other.validates) validates.emplace_back(__v, __a);
                if(__other.default_value) default_value.emplace(*__other.default_value, __a);
            }

            Node(const Node& __other)
            :Node(__other, __other.validates.get_allocator().resource()){}
            
            Node(Node&& __other) ALIB5_NOEXCEPT
            :required(__other.required)
            ,type_restrict(__other.type_restrict)
            ,min_length(std::move(__other.min_length))
            ,max_length(std::move(__other.max_length))
            ,validates(std::move(__other.validates))
            ,default_value(std::move(__other.default_value))
            ,children(std::move(__other.children))
            ,array_subs(std::move(__other.array_subs))
            ,is_tuple(__other.is_tuple)
            ,override_if_conflict(__other.override_if_conflict){}

            Node& operator=(Node&&) ALIB5_NOEXCEPT = default;

            void reset(){
                required = true;
                type_restrict = RNone;
                min_length.set("");
                max_length.set("");
                validates.clear();
                default_value = std::nullopt;
                children.clear();
                array_subs.clear();
                is_tuple = false;
                override_if_conflict = false;
            }

            /// 显式写出 operator= 避免编译器生成被删除的版本
            Node& operator=(const Node& other){
                if(this == &other) [[unlikely]] return *this;
                required = other.required;
                type_restrict = other.type_restrict;
                min_length = other.min_length;
                max_length = other.max_length;
                validates = other.validates;
                default_value = other.default_value;
                children = other.children;
                array_subs = other.array_subs;
                is_tuple = other.is_tuple;
                override_if_conflict = other.override_if_conflict;
                return *this;
            }
        };

        /// 记录错误,operations...
        struct Result{
            bool enable_string_errors;
            bool enable_missing;
            bool success;
            std::pmr::vector<std::pmr::string> recorded_errors;
            std::pmr::unordered_set<
                std::pmr::string,
                detail::TransparentStringHash,
                detail::TransparentStringEqual    
            > missings;

            void reset(){
                success = true;
                recorded_errors.clear();
                missings.clear();
            }

            Result(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
            :recorded_errors(__a)
            ,missings(__a){
                reset();
                enable_string_errors = true;
                enable_missing = true;
            }

            template<class... Args> void record_error(std::string_view fmt,Args&&... args){
                if(enable_string_errors){
                    auto & str = recorded_errors.emplace_back(
                        std::pmr::string(recorded_errors.get_allocator())
                    );
                    std::vformat_to(
                        std::back_inserter(str),
                        fmt,
                        std::make_format_args(args...)
                    );
                }
                success = false;
            }

            void missing_validate(std::string_view m){
                if(enable_missing)missings.emplace(m);
            }
        };

        using ValidateMethod = std::function<bool(AData & node,std::pmr::vector<std::pmr::string> & args)>;

        Node root;
        str::StringPool<std::pmr::string> activated_validates_str_pool;
        std::pmr::unordered_map<std::string_view,ValidateMethod> validates;
        std::pmr::memory_resource * allocator;

        Validator(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :root(__a)
        ,activated_validates_str_pool(__a)
        ,allocator(__a){}

        Validator(const AData & d,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :root(__a)
        ,activated_validates_str_pool(__a)
        ,allocator(__a){
            from_adata(d);
        }

        /// 处理校验方案
        ValidateMethod& emplace_validate_method(std::string_view name,ValidateMethod method){
            return validates.emplace(activated_validates_str_pool.get(name),method).first->second;
        }

        /// 返回错误信息,如果length为0表示没有错误信息
        std::pmr::string ALIB5_API from_adata(const AData & doc);
        
        /// 进行校验,returns false if fails
        bool ALIB5_API validate(AData & doc,Result & result);

        /// 简单包装,returns false if fails
        inline Result validate(AData & doc){
            Result r;
            validate(doc,r);
            return r;
        } 
    };

    namespace debug{
        void ALIB5_API print_validator(const Validator::Node& node, int depth = 0, const std::string& label = "");
    }

    using dvalidator_t = Validator;
    using dvalinode_t = Validator::Node;
    using drestriction_t = Validator::Node::TypeRestrict;
    using dresult_t = Validator::Result;
}

#endif