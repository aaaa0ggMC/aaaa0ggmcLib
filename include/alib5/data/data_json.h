#ifndef ALIB5_ADATA_PL_JSON
#define ALIB5_ADATA_PL_JSON
#include <alib5/data/kernel.h>

namespace alib5::data{
    struct ALIB5_API JSONConfig {
        enum FilterOp{
            Discard,
            Keep
        };

        constexpr static auto sort_asc = [](std::string_view a,std::string_view b){
            return (a <=> b) == std::strong_ordering::less;
        };

        constexpr static auto sort_dsc = [](std::string_view a,std::string_view b){
            return (a <=> b) == std::strong_ordering::greater;
        };

        using CompareFn = bool(std::string_view a,std::string_view b);
        using FilterFn = FilterOp(std::string_view key,const AData & node);

        /// Dump Settings
        size_t dump_indent { 2 };
        char dump_indent_char { ' ' };
        bool compact_lines { false };
        bool compact_spaces { false };
        bool ensure_ascii { false };
        bool warn_when_nan { true };
        int float_precision { -1 };
        std::function<CompareFn> sort_object { nullptr };
        // 返回true保留,false抛弃
        std::function<FilterFn> filter { nullptr }; 

        /// Parse Settings
        /// 默认使用rapidjson默认parse方式--递归处理
        /// 改为false使用模拟栈
        bool rapidjson_recursive { true };
        /// 解析时支持注释
        bool allow_comments { false };
    };

    // 由于使用rapidjson的SAX模式,因此似乎也不需要使用pImpl技术缓存上下文
    struct ALIB5_API JSON{
        using __dump_fn = void(std::string_view,void*);
        enum DumpResult{
            Success,
            EncounteredNAN,
            EncounteredINF
        };

        JSONConfig cfg;
        
        JSON(const JSONConfig& c = JSONConfig()):cfg(c){}
        
        /// returns false if error
        bool ALIB5_API parse(std::string_view data,dadata_t & node);
        
        DumpResult ALIB5_API __internal_dump(__dump_fn fn,void * p,const dadata_t & root);

        template<IsStringLike T> auto dump(T && target,const dadata_t & root){
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