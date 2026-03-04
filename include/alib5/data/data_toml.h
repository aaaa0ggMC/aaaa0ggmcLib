#ifndef ALIB5_ADATA_PL_TOML
#define ALIB5_ADATA_PL_TOML
#include <alib5/data/kernel.h>

namespace alib5::data{
    struct ALIB5_API TOMLConfig {
        
    };

    struct ALIB5_API TOML{
        using __dump_fn = void(std::string_view,void*);

        TOMLConfig cfg;
        
        TOML(const TOMLConfig& c = TOMLConfig()):cfg(c){}
        
        /// returns false if error
        bool ALIB5_API parse(std::string_view data,dadata_t & node);
        
        [[deprecated("This function hasn't been implemented")]]
        void ALIB5_API __internal_dump(__dump_fn fn,void * p,const dadata_t & root);

        
        template<IsStringLike T> 
        [[deprecated("This function hasn't been implemented")]]
        auto dump(T && target,const dadata_t & root){
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