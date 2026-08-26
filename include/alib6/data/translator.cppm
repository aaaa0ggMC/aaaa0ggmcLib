/**
 * @file translator.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief i18n 国际化翻译系统：通用基类、只读平铺翻译器与立体多语言翻译器 (接口定义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <string_view>
#include <string>
#include <format>
#include <utility>
#include <optional>
#include <vector>
#include <unordered_map>
#include <concepts>

export module alib6.data:translator;

import alib6.core;
import :concepts;
import :kernel;
import :json;
import :validator;

namespace pmr = std::pmr;

export namespace alib6::data {

    /// @brief 默认平铺翻译器缓冲区预留大小
    constexpr std::size_t conf_flat_reserve_size = 1024;

    /**
     * @brief 国际化翻译器抽象基类
     */
    struct GenericTranslator {
        pmr::memory_resource* copy_allocator{alib6::get_default_resource()};

        virtual ~GenericTranslator() = default;

        /**
         * @brief 查找翻译文本；若未找到则默认返回 key 原文
         */
        virtual std::string_view get_key_value(std::string_view key) const { return key; }

        /**
         * @brief 查找并格式化翻译内容，直接追加到目标字符串/容器（零多余堆分配）
         */
        template<class Target, class... Args>
        void translate_to(std::string_view keys, Target& target, Args&&... args) const {
            auto fmt = get_key_value(keys);
            if constexpr (sizeof...(Args) > 0) {
                try {
                    std::vformat_to(std::back_inserter(target), fmt, std::make_format_args(args...));
                } catch (...) {
                    target.append(fmt);
                    target.append("[FMT ERROR]");
                }
            } else {
                target.append(fmt);
            }
        }

        /**
         * @brief 查找并格式化翻译内容，返回 PMR 字符串
         */
        template<class... Args>
        pmr::string translate(std::string_view keys, Args&&... args) const {
            return translate_mem(copy_allocator, keys, std::forward<Args>(args)...);
        }

        /**
         * @brief 指定内存池进行格式化翻译，返回 PMR 字符串
         */
        template<class... Args>
        pmr::string translate_mem(pmr::memory_resource* mem, std::string_view keys, Args&&... args) const {
            auto fmt = get_key_value(keys);
            pmr::string res(mem);
            if constexpr (sizeof...(Args) > 0) {
                try {
                    std::vformat_to(std::back_inserter(res), fmt, std::make_format_args(args...));
                } catch (...) {
                    res = fmt;
                    res.append("[FMT ERROR]");
                }
            } else {
                res = fmt;
            }
            return res;
        }

        /**
         * @brief 返回修饰/翻译后的 PMR 字符串
         */
        template<class S, class... Args>
        pmr::string modifier(S&& str, Args&&... args) const {
            return translate(std::string_view(str), std::forward<Args>(args)...);
        }
    };

    class Translator;

    /**
     * @brief 只读平铺翻译器 (单语言快照，高性能且完全线程安全)
     */
    struct FlattenTranslator : public GenericTranslator {
        enum class Type {
            Dots,   ///< 点分格式 ("user.name")
            JsonP   ///< JSON 指针格式 ("/user/name")
        };

        Type type{Type::Dots};

    private:
        friend class Translator;

        pmr::string value_buffer;
        pmr::string key_buffer;
        pmr::unordered_map<std::string_view, std::string_view> mapper;

        struct Offset {
            std::pair<std::size_t, std::size_t> key;
            std::pair<std::size_t, std::size_t> value;
        };
        pmr::vector<Offset> offsets;

        void build_mapper();

        explicit FlattenTranslator(pmr::memory_resource* mem = alib6::get_default_resource())
            : value_buffer(mem), key_buffer(mem), mapper(mem), offsets(mem) {
            copy_allocator = mem;
        }

    public:
        FlattenTranslator(const FlattenTranslator& v);
        FlattenTranslator(FlattenTranslator&& v) noexcept;
        FlattenTranslator& operator=(const FlattenTranslator& v);
        FlattenTranslator& operator=(FlattenTranslator&& v) noexcept;

        std::string_view get_key_value(std::string_view key) const override;

        [[nodiscard]] std::size_t size() const noexcept { return mapper.size(); }
        [[nodiscard]] bool empty() const noexcept { return mapper.empty(); }
    };

    /**
     * @brief 立体多语言翻译器 (支持运行时多语言切换与目录文件批量加载)
     */
    class Translator : public GenericTranslator {
    private:
        AData translations;
        pmr::memory_resource* res;
        AData schema;
        Validator validator;
        Validator::Result vali_result;

        static std::optional<FlattenTranslator> flatten_impl(
            const AData& translations,
            std::string_view target_lang,
            std::string_view current_language,
            FlattenTranslator::Type mode,
            std::size_t reserve_size,
            pmr::memory_resource* alloc_mem
        );

    public:
        pmr::string current_language;

        struct Result {
            int64_t ecode{0}; // 0 为成功
            bool enable_success{true};
            bool enable_failures{true};
            std::size_t success_count{0};
            std::size_t failure_count{0};
            std::vector<std::string> success_files;
            std::vector<std::string> failure_files;

            explicit operator bool() const noexcept { return ecode != 0; }
        };

        using IsTRFileFn = bool (*)(std::string_view);
        static bool default_is_trfile(std::string_view file);

        explicit Translator(pmr::memory_resource* mem = alib6::get_default_resource());

        [[nodiscard]] const AData& data() const noexcept { return translations; }
        [[nodiscard]] AData& data() noexcept { return translations; }

        /**
         * @brief 从内存 JSON 字符串中载入翻译内容并自动校验与合并
         */
        bool load_from_memory(std::string_view data, ErrorWrapper err = {});

        /**
         * @brief 从文件路径或目录中批量加载翻译文件
         */
        Result load_from_entry(
            std::string_view path,
            IsTRFileFn is_trfile = default_is_trfile,
            ErrorWrapper err = {}
        );

        /**
         * @brief 从 FileEntry 中批量加载翻译文件
         */
        Result load_from_entry(
            const io::FileEntry& entry,
            IsTRFileFn is_trfile = default_is_trfile,
            ErrorWrapper err = {}
        );

        /**
         * @brief 切换当前活动语言
         */
        bool switch_language(std::string_view lan);

        /**
         * @brief 使用点分格式 ("user.name") 解析当前语言的翻译
         */
        std::string_view get_key_value_dots(std::string_view key) const;

        /**
         * @brief 使用 JSON 指针格式 ("/user/name") 解析当前语言的翻译
         */
        std::string_view get_key_value(std::string_view key) const override;

        /**
         * @brief 将指定语言或当前语言拍扁为点分格式的 FlattenTranslator
         */
        std::optional<FlattenTranslator> flatten_dots(
            std::string_view key = "",
            std::size_t reserve = conf_flat_reserve_size,
            pmr::memory_resource* mem = nullptr
        ) const;

        /**
         * @brief 将指定语言或当前语言拍扁为 JSON 指针格式的 FlattenTranslator
         */
        std::optional<FlattenTranslator> flatten_jsonp(
            std::string_view key = "",
            std::size_t reserve = conf_flat_reserve_size,
            pmr::memory_resource* mem = nullptr
        ) const;
    };

} // namespace alib6::data
