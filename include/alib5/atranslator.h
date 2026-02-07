/**@file atranslator.h
* @brief 简单的翻译器
* @author aaaa0ggmc
* @date 2026/02/07
* @version 5.0
* @copyright Copyright(c) 2026 
*/
#ifndef ALIB5_ATRANSLATOR_H
#define ALIB5_ATRANSLATOR_H
#include <alib5/adata.h>

namespace alib5{
    constexpr static size_t conf_flat_reserve_size = 1024;

    /// 进行翻译
    struct ALIB5_API GenericTranslator{
        std::pmr::memory_resource * copy_allocator {ALIB5_DEFAULT_MEMORY_RESOURCE};

        virtual ~GenericTranslator() = default;
        virtual std::string_view get_key_value(std::string_view key) const {return key;}
        
        template<IsStringLike T,class... Args>
        inline void translate_to(std::string_view keys,T & target,Args&&... args){
            auto fmt = get_key_value(keys);
            if constexpr (sizeof...(Args)){
                try{
                    // 这里就不转发了
                    std::vformat_to(std::back_inserter(target),fmt,std::make_format_args(args...));
                }catch(...){
                    target += fmt;
                    target.append("[FMT ERROR]");
                }
            }else{
                target.append(fmt);
            }
        }
    
        template<bool copy = true,class... Args>
        inline auto translate(std::string_view keys,Args&&... args){
            auto fmt = get_key_value(keys);
            if constexpr (sizeof...(Args)){
                static thread_local std::string buffer;
                buffer.clear();

                try{
                    // 这里就不转发了
                    std::vformat_to(std::back_inserter(buffer),fmt,std::make_format_args(args...));
                }catch(...){
                    buffer = fmt;
                    buffer.append("[FMT ERROR]");
                    buffer.append(keys);
                }
                if constexpr(copy){
                    return std::pmr::string(buffer,copy_allocator);
                }else return std::string_view(buffer);
            }else{
                if constexpr(copy){
                    return std::pmr::string(fmt,copy_allocator);
                }else return std::string_view(fmt);
            }
        }
        
    };


    /// 平铺的翻译器
    /// 线程安全
    /// 只能从Translator构造出来
    /// 同时只提供一种语言的支持
    /// 具体语义规则看translator调用
    struct ALIB5_API FlattenTranslator : public GenericTranslator{
        enum Type{
            Dots,
            JsonP
        };
        Type type;
    private:
        friend class Translator;

        std::pmr::string value_buffer;
        std::pmr::string key_buffer;
        std::pmr::unordered_map<std::string_view,std::string_view> mapper;

        struct Offset{
            std::pair<size_t,size_t> key;
            std::pair<size_t,size_t> value;
        };
        std::pmr::vector<Offset> offsets;

        FlattenTranslator(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :value_buffer(__a),
        key_buffer(__a),
        mapper(__a),
        offsets(__a){}
        void ALIB5_API build_mapper();
    public:
        FlattenTranslator(const FlattenTranslator & v)
        :type(v.type),
        value_buffer(v.value_buffer),
        key_buffer(v.key_buffer),
        mapper(v.mapper.get_allocator()),
        offsets(v.offsets){
            build_mapper();
        }

        FlattenTranslator(FlattenTranslator && v)
        :type(v.type),
        value_buffer(std::move(v.value_buffer)),
        key_buffer(std::move(v.key_buffer)),
        mapper(std::move(v.mapper)),
        offsets(std::move(v.offsets)){}

        std::string_view get_key_value(std::string_view key) const override {
            auto it = mapper.find(key);
            if(it == mapper.end())return key;
            return it->second;
        }
    };


    /// 立体化的翻译器
    /// 线程不安全
    /// 支持同时持有多种语言
    struct ALIB5_API Translator : GenericTranslator {
    private:
        AData translations;
        std::pmr::memory_resource * res;

        AData schema;
        Validator validator;
        Validator::Result vali_result;
    public:
        std::pmr::string current_language;

        ALIB5_API Translator(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE);

        struct ALIB5_API Result{
            int64_t ecode { err_success };
            bool enable_success {true};
            bool enable_failures {true};

            size_t success_count {0};
            size_t failure_count {0};

            std::vector<std::string> success_files;
            std::vector<std::string> failure_files;

            operator bool(){return ecode != err_success;}
        };
        using IsTRFileFn = bool(*)(std::string_view);
        inline static bool __default_is_trfile(std::string_view file){
            if(file.ends_with(".json"))return true;
            return false;
        }

        inline const AData & data() const {
            return translations;
        }

        /// 从内存中读取
        /// 第一个加载的id会设置为current_language
        bool ALIB5_API load_from_memory(std::string_view data);  
        
        /// 从文件中读取
        /// 第一个加载的id会设置为current_language
        Result ALIB5_API load_from_entry(io::FileEntry entry,IsTRFileFn is_translation_file = __default_is_trfile);  
    
        /// 切换语言
        inline void switch_language(std::string_view lan){
            current_language = lan;
        }

        /// 并没有缓存,使用的是dots格式,见flatten_dots
        std::string_view ALIB5_API get_key_value_dots(std::string_view key) const;

        /// 使用jsonp格式
        std::string_view  get_key_value(std::string_view key) const override {
            auto it = translations.object().find(current_language);
            if(it == translations.object().end())
                return key;
            const AData & current = (*it).second;
            auto ptr = current.jump_ptr(key);
            if(ptr && ptr->is_value())return ptr->value().to<std::string_view>();
            else return key;
        }

        /// "拍扁"目的或者当前翻译为对应的FlattenTranslator,默认分配器选择自己的分配器
        /// 对于冲突字段行为未定义(即无法确定谁覆盖了谁)
        /// 比如  "user.name" : "xxx" , "user" : { "name" : "xx"}
        std::optional<FlattenTranslator> 
        ALIB5_API flatten_dots(std::string_view key = "",size_t reserve = conf_flat_reserve_size,std::pmr::memory_resource * __a = nullptr);

        /// "拍扁"目的或者当前翻译为对应的FlattenTranslator,默认分配器选择自己的分配器
        /// 使用JSON pointer格式,(所以访问的时候记得以"/"开头)
        std::optional<FlattenTranslator> 
        ALIB5_API flatten_jsonp(std::string_view key = "",size_t reserve = conf_flat_reserve_size,std::pmr::memory_resource * __a = nullptr);

    };


}


#endif