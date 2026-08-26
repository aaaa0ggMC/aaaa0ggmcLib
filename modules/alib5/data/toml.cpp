#include <alib5/adata.h>
#include <toml++/toml.hpp>

using namespace alib5;
using namespace alib5::data;

bool TOML::parse(std::string_view data,dadata_t & node){
    try{
        const toml::table table = toml::parse(data);
        struct Frame{
            dadata_t * node;
            const toml::node * table;
        };
        struct ObjFrame{
            size_t index;
            const toml::node * node;
        };
        std::deque<Frame> frames;
        std::vector<ObjFrame> obj_frames;
        frames.emplace_back(&node,&table);

        while(!frames.empty()){
            Frame f = frames.back();
            frames.pop_back();

            // DFS SYNC
            f.table->visit([&](auto&& el) {
                using T = std::decay_t<decltype(el)>;
                if constexpr (toml::is_string<T> || toml::is_integer<T> || toml::is_floating_point<T> || toml::is_boolean<T>) {
                    f.node->rewrite(*el); 
                }else if constexpr (toml::is_date<T> || toml::is_time<T> || toml::is_date_time<T>){
                    // 无奈之举?
                    std::stringstream ss;
                    ss << el;
                    f.node->rewrite(ss.str());
                }else if constexpr (toml::is_table<T>){
                    const toml::table & tb = *f.table->as_table();
                    f.node->set<dobject_t>();
                    auto & obj = f.node->object();
                    obj_frames.clear();
                    for(auto & c : tb){
                        obj_frames.emplace_back(obj.ensure_node(c.first).second,&c.second);
                    }
                    for(int i = obj_frames.size() - 1;i >= 0;--i){
                        frames.emplace_back(&obj.children[obj_frames[i].index],obj_frames[i].node);
                    }
                }else if constexpr(toml::is_array<T>) {
                    const toml::array & tarr = *f.table->as_array();
                    f.node->set<darray_t>();
                    auto & arr = f.node->array();
                    arr.ensure(tarr.size());
                    for(size_t i = 0;i < tarr.size();++i){
                        frames.emplace_back(&arr[i],&tarr[i]);
                    }
                }
            });
        }
    }catch(const toml::parse_error & err){
        return false;
    }
    return true;
}

void ALIB5_API TOML::__internal_dump(__dump_fn fn,void * p,const dadata_t & root){
    bool im_lazy_and_dont_want_to_implement_this_useless_feature_now = true;
    panic_if(im_lazy_and_dont_want_to_implement_this_useless_feature_now,"TOML Dump is forbidden.");
}