/**
 * @file str.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 字符串处理、编码转义、格式转换与字符串池 (接口定义)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <charconv>

export module alib6.core:str;
import std;
import :types;
import :memory;
import :concepts;
import :error;
import :bstd;

namespace pmr = std::pmr;

export namespace alib6::str {

    /**
     * @brief 去除两端空白字符 (返回原 string_view 切片，零分配)
     */
    [[nodiscard]] constexpr std::string_view trim(std::string_view input) noexcept {
        constexpr std::string_view blanks = "\f\v\r\t\n ";
        auto head = input.find_first_not_of(blanks);
        if (head == std::string_view::npos) return "";
        auto tail = input.find_last_not_of(blanks);
        return input.substr(head, tail - head + 1);
    }

    /**
     * @brief 去除左端空白字符
     */
    [[nodiscard]] constexpr std::string_view trim_left(std::string_view input) noexcept {
        constexpr std::string_view blanks = "\f\v\r\t\n ";
        auto head = input.find_first_not_of(blanks);
        if (head == std::string_view::npos) return "";
        return input.substr(head);
    }

    /**
     * @brief 去除右端空白字符
     */
    [[nodiscard]] constexpr std::string_view trim_right(std::string_view input) noexcept {
        constexpr std::string_view blanks = "\f\v\r\t\n ";
        auto tail = input.find_last_not_of(blanks);
        if (tail == std::string_view::npos) return "";
        return input.substr(0, tail + 1);
    }

    /**
     * @brief 分割字符串 (按字符串分隔符，返回 PMR 切片数组)
     */
    [[nodiscard]] pmr::vector<std::string_view> split(
        std::string_view source,
        std::string_view sep,
        memory_resource* mem = get_default_resource()
    );

    /**
     * @brief 分割字符串 (按单字符分隔符)
     */
    [[nodiscard]] pmr::vector<std::string_view> split(
        std::string_view source,
        char sep,
        memory_resource* mem = get_default_resource()
    );

    /**
     * @brief 转大写字符串
     */
    [[nodiscard]] pmr::string to_upper(std::string_view input, memory_resource* mem = get_default_resource());

    /**
     * @brief 转小写字符串
     */
    [[nodiscard]] pmr::string to_lower(std::string_view input, memory_resource* mem = get_default_resource());

    /**
     * @brief 解析转义字符串 (如 \\n -> 换行, \\uXXXX -> UTF-8, \\xHH -> 十六进制)
     */
    [[nodiscard]] pmr::string unescape(std::string_view in, memory_resource* mem = get_default_resource());

    /**
     * @brief 对字符串进行转义序列化
     */
    [[nodiscard]] pmr::string escape(std::string_view in, bool ensure_ascii = false, memory_resource* mem = get_default_resource());

    /**
     * @brief 基于 PMR 的透明哈希字符串常量池
     */
    template<Cast<std::string_view> StringType = pmr::string>
    struct StringPool {
        pmr::unordered_set<
            StringType,
            TransparentStringHash,
            TransparentStringEqual
        > pool;

        explicit StringPool(memory_resource* mem = get_default_resource())
            : pool(mem) {}

        [[nodiscard]] std::string_view get(std::string_view input) {
            auto it = pool.find(input);
            if (it != pool.end()) {
                return *it;
            }
            return *pool.emplace(StringType(input, pool.get_allocator())).first;
        }
    };

} // namespace alib6::str

export namespace alib6::ext {

    /**
     * @brief 将数据安全转换为 PMR 字符串
     */
    template<class T>
    [[nodiscard]] inline pmr::string to_string(const T& v, memory_resource* mem = get_default_resource()) {
        using DecayT = std::decay_t<T>;
        if constexpr (Cast<T, std::string_view>) {
            return pmr::string(std::string_view(v), mem);
        } else if constexpr (std::same_as<DecayT, bool>) {
            return pmr::string(v ? "true" : "false", mem);
        } else if constexpr (std::same_as<DecayT, char>) {
            return pmr::string(1, v, mem);
        } else if constexpr (std::integral<DecayT>) {
            char cbuf[32];
            auto res = std::to_chars(cbuf, cbuf + sizeof(cbuf), v);
            return pmr::string(cbuf, static_cast<usize>(res.ptr - cbuf), mem);
        } else if constexpr (std::floating_point<DecayT>) {
            char cbuf[64];
            auto res = std::to_chars(cbuf, cbuf + sizeof(cbuf), v);
            return pmr::string(cbuf, static_cast<usize>(res.ptr - cbuf), mem);
        } else {
            pmr::string str(mem);
            try {
                std::format_to(std::back_inserter(str), "{}", v);
            } catch (...) {
                str = ALIB6_STR_FAILED_TO_FORMAT;
            }
            return str;
        }
    }

    /**
     * @brief 将字符串转换为基础算术类型或布尔值 (支持 ErrorWrapper 报警)
     */
    template<class T>
    inline auto to_T(
        std::string_view v,
        std::from_chars_result* iresult = nullptr,
        ErrorWrapper err = {}
    ) {
        if constexpr (std::is_same_v<T, bool>) {
            if (v == "true" || v == "1" || v == "TRUE" || v == "True") return true;
            if (v == "false" || v == "0" || v == "FALSE" || v == "False") return false;
            err.report("Invalid boolean literal: '{}'", v);
            return false;
        } else if constexpr (std::is_arithmetic_v<T>) {
            T val = T();
            auto result = std::from_chars(v.data(), v.data() + v.size(), val);
            if (iresult) {
                *iresult = result;
            } else if (result.ec != std::errc()) {
                err.report("Failed to parse number from '{}': {}", v, std::make_error_code(result.ec).message());
            }
            return val;
        } else if constexpr (Cast<std::string_view, T>) {
            return T(v);
        } else {
            static_assert(std::is_arithmetic_v<T>, "Unsupported type for to_T conversion");
        }
    }

} // namespace alib6::ext
