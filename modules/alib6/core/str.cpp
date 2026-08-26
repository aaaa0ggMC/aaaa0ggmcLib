/**
 * @file str.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 字符串分割、转义解析与大小写转换实现
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>
#include <cctype>

module alib6.core;
import std;

namespace pmr = std::pmr;

namespace alib6::str {

    pmr::vector<std::string_view> split(
        std::string_view source,
        std::string_view sep,
        memory_resource* mem
    ) {
        pmr::vector<std::string_view> vec(mem);
        if (sep.empty()) {
            vec.emplace_back(source);
            return vec;
        }

        std::string_view finder = source;
        const usize step = sep.size();

        while (true) {
            auto result = finder.find(sep);
            if (result == std::string_view::npos) {
                vec.emplace_back(finder);
                break;
            }
            vec.emplace_back(finder.substr(0, result));
            finder = finder.substr(result + step);
        }
        return vec;
    }

    pmr::vector<std::string_view> split(
        std::string_view source,
        char sep,
        memory_resource* mem
    ) {
        return split(source, std::string_view(&sep, 1), mem);
    }

    pmr::string to_upper(std::string_view input, memory_resource* mem) {
        pmr::string ret(mem);
        ret.reserve(input.size());
        for (char ch : input) {
            ret.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
        return ret;
    }

    pmr::string to_lower(std::string_view input, memory_resource* mem) {
        pmr::string ret(mem);
        ret.reserve(input.size());
        for (char ch : input) {
            ret.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return ret;
    }

    pmr::string unescape(std::string_view in, memory_resource* mem) {
        pmr::string buffer(mem);
        buffer.reserve(in.size());

        for (usize i = 0; i < in.size(); ++i) {
            if (in[i] == '\\' && i + 1 < in.size()) {
                ++i;
                switch (in[i]) {
                    case 'a':  buffer += '\a'; break;
                    case 'b':  buffer += '\b'; break;
                    case 'f':  buffer += '\f'; break;
                    case 'n':  buffer += '\n'; break;
                    case 'r':  buffer += '\r'; break;
                    case 't':  buffer += '\t'; break;
                    case 'v':  buffer += '\v'; break;
                    case 'e':  buffer += '\x1b'; break;
                    case '\\': buffer += '\\'; break;
                    case '\'': buffer += '\''; break;
                    case '\"': buffer += '\"'; break;
                    case 'x': {
                        if (i + 1 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1]))) {
                            int val = 0;
                            for (int k = 0; k < 2 && i + 1 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])); ++k) {
                                char c = in[++i];
                                val = val * 16 + (std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
                            }
                            buffer += static_cast<char>(val);
                        }
                        break;
                    }
                    case 'u':
                    case 'U': {
                        const int len = (in[i] == 'U') ? 8 : 4;
                        if (i + len < in.size()) {
                            u32 val = 0;
                            bool valid = true;
                            for (int k = 1; k <= len; ++k) {
                                char c = in[i + k];
                                if (std::isxdigit(static_cast<unsigned char>(c))) {
                                    val = val * 16 + (std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
                                } else {
                                    valid = false;
                                    break;
                                }
                            }
                            if (valid) {
                                i += len;
                                if (val <= 0x7F) {
                                    buffer += static_cast<char>(val);
                                } else if (val <= 0x7FF) {
                                    buffer += static_cast<char>(0xC0 | ((val >> 6) & 0x1F));
                                    buffer += static_cast<char>(0x80 | (val & 0x3F));
                                } else if (val <= 0xFFFF) {
                                    buffer += static_cast<char>(0xE0 | ((val >> 12) & 0x0F));
                                    buffer += static_cast<char>(0x80 | ((val >> 6) & 0x3F));
                                    buffer += static_cast<char>(0x80 | (val & 0x3F));
                                } else if (val <= 0x10FFFF) {
                                    buffer += static_cast<char>(0xF0 | ((val >> 18) & 0x07));
                                    buffer += static_cast<char>(0x80 | ((val >> 12) & 0x3F));
                                    buffer += static_cast<char>(0x80 | ((val >> 6) & 0x3F));
                                    buffer += static_cast<char>(0x80 | (val & 0x3F));
                                }
                                break;
                            }
                        }
                        buffer += in[i];
                        break;
                    }
                    default: {
                        if (in[i] >= '0' && in[i] <= '7') {
                            int val = 0;
                            for (int k = 0; k < 3 && i < in.size() && (in[i] >= '0' && in[i] <= '7'); ++k) {
                                val = val * 8 + (in[i++] - '0');
                            }
                            --i;
                            buffer += static_cast<char>(val);
                        } else {
                            buffer += in[i];
                        }
                    }
                }
            } else {
                buffer += in[i];
            }
        }
        return buffer;
    }

    pmr::string escape(std::string_view in, bool ensure_ascii, memory_resource* mem) {
        pmr::string buffer(mem);
        buffer.reserve(in.size() + 16);
        constexpr const char* hex = "0123456789abcdef";

        for (usize i = 0; i < in.size(); ++i) {
            const auto c = static_cast<unsigned char>(in[i]);
            switch (c) {
                case '\"': buffer += "\\\""; break;
                case '\\': buffer += "\\\\"; break;
                case '\a': buffer += "\\a";  break;
                case '\b': buffer += "\\b";  break;
                case '\f': buffer += "\\f";  break;
                case '\n': buffer += "\\n";  break;
                case '\r': buffer += "\\r";  break;
                case '\t': buffer += "\\t";  break;
                case '\v': buffer += "\\v";  break;
                case '\x1b': buffer += "\\e"; break;
                default: {
                    if (c < 32 || c == 127) {
                        buffer += "\\x";
                        buffer += hex[(c >> 4) & 0xF];
                        buffer += hex[c & 0xF];
                    } else if (ensure_ascii && c >= 0x80) {
                        u32 cp = 0;
                        usize len = 0;
                        if ((c & 0xE0) == 0xC0 && i + 1 < in.size()) {
                            cp = (c & 0x1F) << 6 | (static_cast<unsigned char>(in[++i]) & 0x3F);
                            len = 4;
                        } else if ((c & 0xF0) == 0xE0 && i + 2 < in.size()) {
                            cp = (c & 0x0F) << 12 | ((static_cast<unsigned char>(in[i+1]) & 0x3F) << 6) | (static_cast<unsigned char>(in[i+2]) & 0x3F);
                            i += 2; len = 4;
                        } else if ((c & 0xF8) == 0xF0 && i + 3 < in.size()) {
                            cp = (c & 0x07) << 18 | ((static_cast<unsigned char>(in[i+1]) & 0x3F) << 12) | ((static_cast<unsigned char>(in[i+2]) & 0x3F) << 6) | (static_cast<unsigned char>(in[i+3]) & 0x3F);
                            i += 3; len = 8;
                        }

                        if (len == 4) {
                            buffer += "\\u";
                            for (int k = 12; k >= 0; k -= 4) buffer += hex[(cp >> k) & 0xF];
                        } else if (len == 8) {
                            buffer += "\\U";
                            for (int k = 28; k >= 0; k -= 4) buffer += hex[(cp >> k) & 0xF];
                        } else {
                            buffer += "\\x";
                            buffer += hex[(c >> 4) & 0xF];
                            buffer += hex[c & 0xF];
                        }
                    } else {
                        buffer += static_cast<char>(c);
                    }
                    break;
                }
            }
        }
        return buffer;
    }

} // namespace alib6::str
