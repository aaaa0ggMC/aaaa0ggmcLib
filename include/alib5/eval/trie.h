#ifndef ALIB5_AEVAL_TRIE
#define ALIB5_AEVAL_TRIE
#include <alib5/autil.h>
#include <alib5/adebug.h>
#include <cstring>

namespace alib5::eval::detail{
    constexpr static size_t mapper_invalid = std::numeric_limits<size_t>::max();
    template<class T> concept CanToSizeType = requires(T & t){
        (size_t)t;
    };

    // 如果选用sparse那么则会选择umap映射
    template<CanToSizeType T = char , bool sparse = false >
    struct ALIB5_API TrieMapper{
    private:
        constexpr static size_t conf_charset_mapper_size = 1ULL << (sizeof(T) * 8);
        static_assert(sparse || conf_charset_mapper_size <= 65536, "Data type too large for static mapper!");

        struct ALIB5_API Trie{
            std::pmr::vector<Trie> trie;
            
            size_t size;
            
            std::pmr::vector<size_t> mapping_ids;
            
            Trie(size_t size,std::pmr::memory_resource * __a)
            :trie(__a)
            ,mapping_ids(__a){
                this->size = size;
            }

            auto& get_mapping(){
                return mapping_ids;
            }

            void add_mapping(size_t id){
                mapping_ids.push_back(id);
            }

            Trie& operator[](size_t index){
                if(trie.size() == 0)trie.resize(size,Trie(size,trie.get_allocator().resource()));
                panic_debug(index >= trie.size(), "Array out of bounds!");
                return trie[index];
            }

            bool empty() const {
                return trie.empty();
            }

            const Trie& operator[](size_t index) const {
                panic_debug(index >= trie.size(), "Array out of bounds!");
                return trie[index];
            }
        };
        Trie trie;
        bool use_mapper;
        using mapper_t = std::conditional_t<
            sparse,
            std::unordered_map<size_t,size_t>,
            std::array<size_t,conf_charset_mapper_size>
        >;
        mapper_t charset_mapper;
        

        size_t get_index(size_t ch) const {
            if(use_mapper){
                if constexpr(sparse){
                    auto it = charset_mapper.find(ch);
                    if(it != charset_mapper.end())return it->second;
                    return mapper_invalid;
                }else{
                    return charset_mapper[ch];
                }
            }else{
                return ch;
            }
        }

    public:
        TrieMapper(std::span<T> charset,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :trie(charset.size(),__a){
            use_mapper = true;
            if constexpr(!sparse){
                charset_mapper.fill(mapper_invalid);
            }
            // 初始化charset
            size_t i = 0;
            for(auto & c : charset){
                charset_mapper[(size_t)c] = i++;
            }
        }
        TrieMapper(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :trie(conf_charset_mapper_size,__a){
            // 这个是否反正也没有初始化,这里恰好可以进行判断
            panic_if(conf_charset_mapper_size >= 65536,"Data type too large for full mapper!");
            use_mapper = false;
        }

        bool add(std::span<const T> str,size_t mapping_id){
            if(str.empty()){
                invoke_error(err_empty_data,"String is empty!");
                return false;
            }
            Trie * t = &trie;
            for(auto & ch : str){
                size_t index = get_index((size_t)ch);
                if(index == mapper_invalid){
                    invoke_error(err_not_in_charset,"Char \'{}\' is not in the charset!",ch);
                    return false;
                }
                t = &((*t)[index]);
            }
            std::cout << "Add mapping:" << mapping_id << std::endl;
            t->add_mapping(mapping_id);
            return true;
        }

        std::pair<std::span<const size_t>,size_t> get(std::span<const T> str) const {
            if(str.empty()){
                return {};
            }
            const Trie * t = &trie;
            const Trie * last_valid = nullptr;
            size_t last_len = 0;

            size_t i = 0;
            for(;i < str.size();++i){
                auto & c = str[i];
                if(t->empty()){
                    if(last_valid)return std::make_pair(last_valid->mapping_ids,last_len);
                    else return std::make_pair(std::span<const size_t>(), i + 1);
                }
                size_t index = get_index((size_t)c);
                if(index == mapper_invalid){
                    invoke_error(err_not_in_charset,"Char \'{}\' is not in the charset!",c);
                    return std::make_pair(std::span<const size_t>(), i + 1);
                }
                t = &((*t)[index]);
                if(!t->mapping_ids.empty()){
                    last_valid = t;
                    last_len = i + 1;
                }
            }
            std::cout << std::format("MP:{}",t->mapping_ids) << std::endl;
            return std::make_pair(std::span(t->mapping_ids),i);
        }
    };
}


#endif