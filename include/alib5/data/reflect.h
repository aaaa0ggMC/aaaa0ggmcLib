/**
 * @file reflect.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Reflect of adata,works fine with GCC 16.1.1
 * @version 5.0
 * @date 2026-05-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_DATA_REFLECT
#define ALIB5_DATA_REFLECT
#include <alib5/data/reflect/kernel.h>
#include <alib5/data/kernel.h>
#include <alib5/autil.h>
#include <limits.h>
#include <meta>

#include <alib5/data/reflect/from_adata.h>
#include <alib5/data/reflect/to_adata.h>
#include <alib5/data/reflect/gen_schema.h>

namespace alib5{
    template<class InT> 
    void from_adata(InT & fill_data,const AData & root);

    template<class InT> 
    InT from_adata(const AData & root);

    template<class InT> 
    AData to_adata(InT && base);

    template<class T>
    AData generate_schema();
}

//// Implementation
namespace alib5{
    template<class InT> 
    void from_adata(InT & fill_data,const AData & root){
        detail::_from_adata(fill_data,root);
    }

    template<class InT> 
    InT from_adata(const AData & root){
        InT object;
        detail::_from_adata(object,root);
        return object;
    }

    template<class InT> 
    AData to_adata(InT && base){
        return detail::_to_adata(std::forward<InT>(base));
    }

    template<class T>
    AData generate_schema(){
        return detail::_generate_schema<T>();
    }

}
#endif