/**
 * @file reflect_v2.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 速度反而慢了。。。。。慢了60%。。。我还是不打算开发了。。。
 * @version 5.0
 * @date 2026-05-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_ADATA_REFLECT_V2
#define ALIB5_ADATA_REFLECT_V2
#include <alib5/data/kernel.h>

namespace alib5::v2{
    template<class T>
    AData to_adata(const T & val);
};

namespace alib5::v2::type_to{
    struct FieldMetaData{
        std::string_view name;
    }; 

    struct FieldDescriptor{
        using SerializeFn = void (*) (const void * parent_ptr, AData & root,const FieldDescriptor& desc);
        std::string_view name;
        FieldMetaData metadata;

        SerializeFn serialize_fn;
    };

    template<class T,std::meta::info field_info>
    void field_serialize_impl(const void * parent_ptr,AData & root,const FieldDescriptor& desc){
        const T& obj = *static_cast<const T*>(parent_ptr);

        const auto & field_val = obj.[: field_info :];

        AData child = alib5::v2::to_adata(field_val);

        root[desc.name] = std::move(child);
    }

    template<std::meta::info info>
    constexpr FieldMetaData get_field_meta_data(){
        FieldMetaData metadata;
        metadata.name = std::meta::identifier_of(info);
        return metadata;
    }

    template<class T>
    struct ClassDescriptors{
        static consteval auto build(){
            constexpr auto info = ^^T;
            constexpr auto context = std::meta::access_context::unchecked();
            constexpr static auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(info,context)
            );
            constexpr auto descriptor_size = members.size();
        
            std::array<FieldDescriptor,descriptor_size> descriptors;

            size_t idx = 0;
            template for(constexpr auto member : members){
                constexpr FieldMetaData metadata = get_field_meta_data<member>();

                descriptors[idx] = FieldDescriptor{
                    metadata.name,
                    metadata,
                    &field_serialize_impl<T,member>
                };
                ++idx;
            }

            return descriptors;
        }

        static constexpr auto value = build();
    };
};


namespace alib5::v2{
    template<class T>
    AData to_adata(const T & val){
        constexpr auto t_info = ^^T;
        AData root;

        if constexpr(requires{ root = val; }){
            root = val;
        }else if constexpr (
            requires{
                { val.size() } -> std::convertible_to<std::size_t>;
                val[0];
            }
        ){
            root.set<darray_t>();
            auto & array = root.array();
            array.reserve(val.size());

            for(size_t i = 0;i < val.size();++i){
                array[i] = to_adata(val[i]);
            }
        }else if constexpr (
            std::meta::is_class_type(t_info)
        ){
            root.set<dobject_t>();

            constexpr auto & descriptors = type_to::ClassDescriptors<T>::value;

            for(const auto & desc : descriptors){
                desc.serialize_fn(&val,root,desc);
            }
        }else{
            static_assert(std::meta::is_class_type(t_info),"Unsupported Structure!");
        }
        return root;
    }

}


#endif

/**
 * @brief 补充： 我都还没写到enum以及写到重头戏attributes,这个性能就比不上我现在的版本了，那我的选择不言而喻
 -----------------------
V2

TimeCost         : 16.118517s
RunTimes         : 100000
Average          : 161.185167us
ShortestAvg      : 154.457400us
LongestAvg       : 229.851690us
Stddev           : 0.007429
CV               : 4.6090%
--------------------------------

-----------------------
V1

TimeCost         : 8.441978s
RunTimes         : 100000
Average          : 84.419781us
ShortestAvg      : 81.770170us
LongestAvg       : 119.550430us
Stddev           : 0.002867
CV               : 3.3960%
--------------------------------

#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/aperf.h>
#include <alib5/compact/manip_table.h>
using namespace alib5;

struct E{
    std::string a = "hello";
    std::string b = "hello";
    std::string c = "hello";
    std::string d = "hello";
    std::string e = "hello";
    std::string f = "hello";

    std::vector<std::string> omg = {
        "1",
        "2",
        "3",
        "4",
        "5"
    };
};

struct C{
    float a = 1;
    float b = 1;
    float c = 1;
    float d = 1;
    float e = 1;
    float f = 1;
    float g = 1;
    float h = 1;

    E hh;
};

struct B{
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;
    int f = 0;
    int g = 0;
    int h = 0;
    int i = 0;

    C c1;
    C c2;
};

struct A{
    B b1;
    B b2;

    std::vector<E> es = {
        E(),E(),E()
    };
};

int main(){
    A a;

    aout << Benchmark([&]{
        v2::to_adata(a);
    }).run(100,1000).name("V2") << fls;


    aout << Benchmark([&]{
        alib5::to_adata(a);
    }).run(100,1000).name("V1") << fls;
}
 * 
 */