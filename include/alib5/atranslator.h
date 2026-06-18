/**
 * @file atranslator.h
 * @brief i18n translators: generic base, flat (single-language) and structured (multi-language) variants. / 翻译器：通用基类、平铺（单语言）与立体（多语言）两种实现。
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 *
 * @par Original Comment:
 * 简单的翻译器
 */
#ifndef ALIB5_ATRANSLATOR_H
#define ALIB5_ATRANSLATOR_H
#include <alib5/adata.h>

namespace alib5{
    /// @brief Default capacity reserved for flat translator buffers. 默认flat缓冲区大小
    constexpr static size_t conf_flat_reserve_size = 1024;

    /// @brief Base translator exposing get_key_value plus format-aware translate/modifier helpers. 进行翻译
    struct ALIB5_API GenericTranslator{
        /// @brief PMR allocator used when returning owning copies of translated strings. 复制时使用的分配器
        std::pmr::memory_resource * copy_allocator {ALIB5_DEFAULT_MEMORY_RESOURCE};

        /// @brief Virtual destructor for safe polymorphic deletion.
        virtual ~GenericTranslator() = default;
        /// @brief Return the translated value for @p key, or @p key itself if absent. Subclasses override.
        virtual std::string_view get_key_value(std::string_view key) const {return key;}

        /// @brief Look up @p keys and append the formatted result to @p target, with [FMT ERROR] fallback. 翻译并写入目标
        ///
        /// @par Original Comment:
        /// (inline step comments retained below)
        ///
        /// @tparam T Target string-like type exposing append().
        /// @tparam Args Format argument types.
        /// @param keys Translation key.
        /// @param target Output container to append into.
        /// @param args Format arguments.
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

        /// @brief Return a translated, owning copy of @p str formatted with @p args. 返回修改过的字符串
        ///
        /// @par Original Comment:
        /// (delegates to translate<true>)
        ///
        /// @tparam S Source string-like type.
        /// @tparam Args Format argument types.
        /// @param str Source key.
        /// @param args Format arguments.
        template<IsStringLike S,class... Args>
        inline auto modifier(S && str,Args&&... args){
            return translate<true>(std::string_view(str),std::forward<Args>(args)...);
        }

        /// @brief Look up @p keys and return either an owning copy or a string_view, formatted with @p args. 翻译
        ///
        /// @par Original Comment:
        /// (inline step comments retained below)
        ///
        /// @tparam copy When true, return an owning std::pmr::string; when false, return a string_view into a thread-local buffer.
        /// @tparam Args Format argument types.
        /// @param keys Translation key.
        /// @param args Format arguments.
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


    /// @brief Flat single-language translator built once from a structured Translator; thread-safe. 平铺的翻译器
    ///
    /// @par Original Comment:
    /// 线程安全
    /// 只能从Translator构造出来
    /// 同时只提供一种语言的支持
    /// 具体语义规则看translator调用
    struct ALIB5_API FlattenTranslator : public GenericTranslator{
        /// @brief Flattening strategy used to build the key map. 格式
        enum Type{
            /// @brief Dotted key format ("a.b.c").
            Dots,
            /// @brief JSON pointer format ("/a/b/c").
            JsonP
        };
        /// @brief Active flattening type for this instance. 格式类型
        Type type;
    private:
        friend class Translator;

        /// @brief Backing buffer holding concatenated values. 值缓冲区
        std::pmr::string value_buffer;
        /// @brief Backing buffer holding concatenated keys. 键缓冲区
        std::pmr::string key_buffer;
        /// @brief Map of key view -> value view into the backing buffers. 映射表
        std::pmr::unordered_map<std::string_view,std::string_view> mapper;

        /// @brief Records [start,len) pairs for each key and value within the backing buffers. 偏移记录
        struct Offset{
            /// @brief Key span within key_buffer. key位置
            std::pair<size_t,size_t> key;
            /// @brief Value span within value_buffer. value位置
            std::pair<size_t,size_t> value;
        };
        /// @brief Offsets used to (re)build mapper after copies. 偏移列表
        std::pmr::vector<Offset> offsets;

        /// @brief Rebuild mapper string_views from offsets after a copy. 重建mapper
        void ALIB5_API build_mapper();

        /// @brief Private constructor; only Translator constructs flat translators. 构造
        FlattenTranslator(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        :value_buffer(__a),
        key_buffer(__a),
        mapper(__a),
        offsets(__a){}
    public:
        /// @brief Copy constructor: copies buffers and rebuilds mapper from offsets. 拷贝构造
        FlattenTranslator(const FlattenTranslator & v)
        :type(v.type),
        value_buffer(v.value_buffer),
        key_buffer(v.key_buffer),
        mapper(v.mapper.get_allocator()),
        offsets(v.offsets){
            build_mapper();
        }

        /// @brief Move constructor: steals buffers, mapper, and offsets directly. 移动构造
        FlattenTranslator(FlattenTranslator && v)
        :type(v.type),
        value_buffer(std::move(v.value_buffer)),
        key_buffer(std::move(v.key_buffer)),
        mapper(std::move(v.mapper)),
        offsets(std::move(v.offsets)){}

        /// @brief Return the value for @p key, or @p key itself if not in mapper. 查找
        std::string_view get_key_value(std::string_view key) const override {
            auto it = mapper.find(key);
            if(it == mapper.end())return key;
            return it->second;
        }
    };


    /// @brief Structured multi-language translator backed by AData; not thread-safe. 立体化的翻译器
    ///
    /// @par Original Comment:
    /// 线程不安全
    /// 支持同时持有多种语言
    struct ALIB5_API Translator : GenericTranslator {
    private:
        /// @brief All loaded translations keyed by language id. 翻译数据
        AData translations;
        /// @brief Memory resource used by this translator. 分配器
        std::pmr::memory_resource * res;

        /// @brief Schema used to validate translation files. schema
        AData schema;
        /// @brief Validator bound to @ref schema. validator
        Validator validator;
        /// @brief Cached result of the last validation run. 验证结果
        Validator::Result vali_result;
    public:
        /// @brief Currently active language id; set by the first load and switch_language. 当前语言
        std::pmr::string current_language;

        /// @brief Construct a translator bound to memory resource @p __a. 构造
        ALIB5_API Translator(std::pmr::memory_resource* __a = ALIB5_DEFAULT_MEMORY_RESOURCE);

        /// @brief Outcome of a batch load: counts and file lists for success and failure. 加载结果
        struct ALIB5_API Result{
            /// @brief Aggregate error code; err_success means no error. 错误码
            int64_t ecode { err_success };
            /// @brief Whether to record successful files. 记录成功
            bool enable_success {true};
            /// @brief Whether to record failed files. 记录失败
            bool enable_failures {true};

            /// @brief Count of files loaded successfully. 成功计数
            size_t success_count {0};
            /// @brief Count of files that failed to load. 失败计数
            size_t failure_count {0};

            /// @brief Paths of successfully loaded files. 成功文件列表
            std::vector<std::string> success_files;
            /// @brief Paths of files that failed to load. 失败文件列表
            std::vector<std::string> failure_files;

            /// @brief Truthiness: true when an error occurred (ecode != err_success). 转化为bool
            operator bool(){return ecode != err_success;}
        };
        /// @brief Function pointer predicate deciding whether a file is a translation file. 判断是否为翻译文件
        using IsTRFileFn = bool(*)(std::string_view);
        /// @brief Default predicate: accept files ending in ".json". 默认判断函数
        inline static bool __default_is_trfile(std::string_view file){
            if(file.ends_with(".json"))return true;
            return false;
        }

        /// @brief Read-only access to the underlying translations data. 获取数据
        inline const AData & data() const {
            return translations;
        }

        /// @brief Parse translations from an in-memory blob; the first loaded id becomes current_language. 从内存中读取
        ///
        /// @par Original Comment:
        /// 第一个加载的id会设置为current_language
        bool ALIB5_API load_from_memory(std::string_view data);

        /// @brief Load translations from a directory entry, accepting files matched by @p is_translation_file. 从文件中读取
        ///
        /// @par Original Comment:
        /// 第一个加载的id会设置为current_language
        Result ALIB5_API load_from_entry(io::FileEntry entry,IsTRFileFn is_translation_file = __default_is_trfile);

        /// @brief Switch the active language to @p lan; returns false if unknown. 切换语言
        inline bool switch_language(std::string_view lan){
            auto & o = translations.object();
            if(o.find(lan) == o.end())return false;
            current_language = lan;
            return true;
        }

        /// @brief Resolve @p key using dotted format against the current language; no caching. dots格式查找
        ///
        /// @par Original Comment:
        /// 并没有缓存,使用的是dots格式,见flatten_dots
        std::string_view ALIB5_API get_key_value_dots(std::string_view key) const;

        /// @brief Resolve @p key using JSON pointer format against the current language. jsonp格式查找
        ///
        /// @par Original Comment:
        /// 使用jsonp格式
        std::string_view  get_key_value(std::string_view key) const override {
            auto it = translations.object().find(current_language);
            if(it == translations.object().end())
                return key;
            const AData & current = it.second();
            auto ptr = current.jump_ptr(key);
            if(ptr && ptr->is_value())return ptr->value().to<std::string_view>();
            else return key;
        }

        /// @brief Flatten the current (or named) language into a FlattenTranslator using dotted keys. "拍扁"为dots格式
        ///
        /// @par Original Comment:
        /// "拍扁"目的或者当前翻译为对应的FlattenTranslator,默认分配器选择自己的分配器
        /// 对于冲突字段行为未定义(即无法确定谁覆盖了谁)
        /// 比如  "user.name" : "xxx" , "user" : { "name" : "xx"}
        std::optional<FlattenTranslator>
        ALIB5_API flatten_dots(std::string_view key = "",size_t reserve = conf_flat_reserve_size,std::pmr::memory_resource * __a = nullptr);

        /// @brief Flatten the current (or named) language into a FlattenTranslator using JSON pointer keys. "拍扁"为jsonp格式
        ///
        /// @par Original Comment:
        /// "拍扁"目的或者当前翻译为对应的FlattenTranslator,默认分配器选择自己的分配器
        /// 使用JSON pointer格式,(所以访问的时候记得以"/"开头)
        std::optional<FlattenTranslator>
        ALIB5_API flatten_jsonp(std::string_view key = "",size_t reserve = conf_flat_reserve_size,std::pmr::memory_resource * __a = nullptr);

    };
}


#endif
