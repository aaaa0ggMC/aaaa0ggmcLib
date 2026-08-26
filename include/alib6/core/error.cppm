/**
 * @file error.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 错误处理&传递
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <alib6/debug.h>

export module alib6.core:error;
import std;
import :memory;
import :types;
import :concepts;
import :debug;

namespace pmr = std::pmr;

export namespace alib6{
    
    /// 附带源码位置的错误码包装
    struct CodeWithLocation {
        i64 code;
        std::source_location location;

        CodeWithLocation(i64 c = 0, std::source_location loc = std::source_location::current()) noexcept
            : code(c), location(loc) {}

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "[Code {} at {}:{}]", code, location.file_name(), location.line());
        }
    };

    struct ErrorRecord{
        // 错误信息
        pmr::string message;
        // 错误代码
        i64 code;
        // 源码位置
        std::source_location location;

        ErrorRecord(pmr::string && message, i64 code, std::source_location loc = std::source_location::current())
            : message(std::move(message))
            , code(code) 
            , location(loc) {}

        template<class Target>
        void write_to_log(Target& target) const {
            std::format_to(std::back_inserter(target), "[Error code={}] {} at {}:{}", code, message, location.file_name(), location.line());
        }
    };


    struct Error{
    private:
        memory_resource* res;
    public:
        using CodeWithLocation = alib6::CodeWithLocation;

        // This vector never shrinks
        // If it shrinks, mstd::string would deallocate, which leads memory "wasted"
        pmr::vector<
            ErrorRecord
        > messages;
        usize cursor { 0 };

        Error(memory_resource * mem = get_default_resource())
            : messages(mem), res(mem) {}

        [[nodiscard]] auto get_allocator() const noexcept { return res; }

        /// 清除目前的错误信息
        void clear() noexcept { cursor = 0; }
        
        /// 增加新的错误信息
        void report(pmr::string && str, CodeWithLocation cc = CodeWithLocation()){
            if(messages.size() > cursor){
                messages[cursor].message = std::move(str);
                messages[cursor].code = cc.code;
                messages[cursor].location = cc.location;
            }else{
                messages.emplace_back(
                    std::move(str),
                    cc.code,
                    cc.location
                );
            }
            ++cursor;
        }
        
        template<Cast<std::string_view> T>
        void report(T value, CodeWithLocation cc = CodeWithLocation()){
            report(
                std::move(pmr::string(
                    std::string_view(value),
                    get_allocator()
                )),
                cc
            );
        }

        template<class... Args> void report(
            CodeWithLocation cc,
            const std::format_string<Args...> & fmt, 
            Args&&... args
        ){
            pmr::string str(get_allocator());

            try{
                std::format_to(std::back_inserter(str), fmt, std::forward<Args>(args)...);
            }catch(...){
                str = ALIB6_STR_FAILED_TO_FORMAT "error report.";
            }

            report(std::move(str), cc);
        }

        template<class... Args> void report(
            PanicFormat<std::type_identity_t<Args>...> fmt, 
            Args&&... args
        ){
            report(CodeWithLocation(0, fmt.loc), fmt.fmt, std::forward<Args>(args)...);
        }

        /// 是否有错误信息
        [[nodiscard]] bool has_error() const noexcept { return cursor > 0; }
        [[nodiscard]] explicit operator bool() const noexcept { return has_error(); }
        [[nodiscard]] usize size() const noexcept { return cursor; }
        [[nodiscard]] usize capacity() const noexcept { return messages.capacity(); }

        [[nodiscard]] const ErrorRecord& operator[](usize idx) const { return messages[idx]; }
        [[nodiscard]] ErrorRecord& operator[](usize idx) { return messages[idx]; }

        // Iterate
        auto begin() const noexcept { return messages.begin(); }
        auto end() const noexcept { return messages.begin() + cursor; }
        auto cbegin() const noexcept { return begin(); }
        auto cend() const noexcept { return end(); }
    };


    struct ErrorWrapper {
    private:
        Error * err;
    public:
        ErrorWrapper():err{ nullptr }{}
        ErrorWrapper(Error & ctx):err{ &ctx }{}
        ErrorWrapper(Error && ctx) = delete("Rvalue is not allowed!");
        ErrorWrapper(const Error & ctx) = delete("Const lvalue is not allowed!");

        constexpr static bool __need_invoke_error() noexcept {
#if defined(ALIB6_FLAG_ERROR_EXCEPTION)
                return true;
#elif defined(ALIB6_FLAG_ERROR_PANIC)
                return true;
#else
                return false;
#endif
        }

        void __invoke_error(std::string_view str, CodeWithLocation cc){
#if defined(ALIB6_FLAG_ERROR_EXCEPTION)
                throw std::runtime_error(std::string(str));
#elif defined(ALIB6_FLAG_ERROR_PANIC)
                panicf(
                    PanicFormat("Message: {}\nCode: {}\n", cc.location),
                    str, cc.code
                );
#else
                // 啥也不干
#endif
        }

        /// 增加新的错误信息
        template<class T>
        void __report(T && str, CodeWithLocation cc = CodeWithLocation()){
            if(err){
                err->report(std::forward<T>(str), cc);
            }else{
                __invoke_error(str, cc);
            }
        }

        /// 支持右值 pmr::string 零拷贝移动
        void report(pmr::string && str, CodeWithLocation cc = CodeWithLocation()){
            __report(std::move(str), cc);
        }
        
        template<Cast<std::string_view> T>
        void report(T value, CodeWithLocation cc = CodeWithLocation()){
            __report(std::string_view(value), cc);
        }

        template<class... Args> void report(
            CodeWithLocation cc,
            const std::format_string<Args...> & fmt, 
            Args&&... args
        ){
            if(err || __need_invoke_error()){
                pmr::string str(err ? err->get_allocator() : get_default_resource());

                try{
                    std::format_to(std::back_inserter(str), fmt, std::forward<Args>(args)...);
                }catch(...){
                    str = ALIB6_STR_FAILED_TO_FORMAT "error report.";
                }

                __report(std::move(str), cc);
            }
        }

        template<class... Args> void report(
            PanicFormat<std::type_identity_t<Args>...> fmt, 
            Args&&... args
        ){
            report(CodeWithLocation(0, fmt.loc), fmt.fmt, std::forward<Args>(args)...);
        }

    };

}
