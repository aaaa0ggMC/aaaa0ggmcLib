#ifndef ALIB5_ADATA_PL_JSON
#define ALIB5_ADATA_PL_JSON
#include <alib5/data/kernel.h>

namespace alib5::data{
    struct ALIB5_API JSONConfig {
        using CompareFn = bool(*)(std::string_view a,std::string_view b);
        using FilterFn = bool(*)(std::string_view key,const AData & node);

        /// Dump Settings
        size_t dump_indent { 2 };
        char dump_indent_char { ' ' };
        bool compact_lines { false };
        bool compact_spaces { false };
        CompareFn sort_object { nullptr };
        // 返回true保留,false抛弃
        FilterFn filter { nullptr }; 

        /// Parse Settings
        // none
    };

    // 由于使用rapidjson的SAX模式,因此似乎也不需要使用pImpl技术缓存上下文
    struct ALIB5_API JSON{
        using __dump_fn = void(std::string_view,void*);

        JSONConfig cfg;
        
        JSON(const JSONConfig& c = JSONConfig()):cfg(c){}
        
        bool ALIB5_API parse(std::string_view data,dadata_t & node);
        
        bool ALIB5_API __internal_dump(__dump_fn fn,void * p,dadata_t & root);

        template<IsStringLike T> bool dump(T && target,dadata_t & root){
            auto fn = [](std::string_view sv, void* ag){
                using type = std::decay_t<T>;
                type & out = *static_cast<type*>(ag);
                if constexpr (requires { out.append(sv); }){
                    out.append(sv);
                }else{
                    out += sv; 
                }
            };
            return __internal_dump(fn, &target, root);
        }
    };
}

#endif