/**
 * @file reflect.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief AData reflection support.
 * 
 * \par Original Comment
 * English: Reflect of adata, works fine with GCC 16.1.1
 * Chinese: Adata的反射，在GCC 16.1.1下工作良好
 * 
 * @version 5.0
 * @date 2026-05-10
 * @copyright Copyright (c) 2026
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

namespace alib5 {

    /**
     * @brief Deserializes an AData object into an existing C++ data structure.
     * 
     * @tparam InT Type of the target data structure.
     * @param fill_data The data structure to populate.
     * @param root The source AData object.
     */
    template<class InT> 
    void from_adata(InT & fill_data, const AData & root);

    /**
     * @brief Deserializes an AData object and returns a new C++ data structure.
     * 
     * @tparam InT Type of the target data structure.
     * @param root The source AData object.
     * @return InT The populated data structure.
     */
    template<class InT> 
    InT from_adata(const AData & root);

    /**
     * @brief Serializes a C++ data structure into an AData object.
     * 
     * @tparam InT Type of the source data structure.
     * @param base The source data structure to serialize.
     * @return AData The resulting AData object.
     */
    template<class InT> 
    AData to_adata(InT && base);

    /**
     * @brief Generates an AData schema representation for a specific type.
     * 
     * @tparam T Type to generate the schema for.
     * @return AData The generated schema object.
     */
    template<class T>
    AData generate_schema();

}

// ============================================================================
// Implementation
// ============================================================================

namespace alib5 {

    template<class InT> 
    inline void from_adata(InT & fill_data, const AData & root) {
        detail::_from_adata(fill_data, root);
    }

    template<class InT> 
    inline InT from_adata(const AData & root) {
        InT object;
        detail::_from_adata(object, root);
        return object;
    }

    template<class InT> 
    inline AData to_adata(InT && base) {
        return detail::_to_adata(std::forward<InT>(base));
    }

    template<class T>
    inline AData generate_schema() {
        return detail::_generate_schema<T>();
    }

}

#endif