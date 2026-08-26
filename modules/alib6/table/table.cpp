/**
 * @file table.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 现代化表格生成与排版引擎实现 (alib6.table)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

module alib6.table;
import std;

namespace pmr = std::pmr;

namespace alib6::table {

    class Utf8Toolkit {
    public:
        static usize get_display_width(std::string_view sv) noexcept {
            usize total_width = 0;
            const unsigned char* p = reinterpret_cast<const unsigned char*>(sv.data());
            const unsigned char* end = p + sv.size();

            while (p < end) {
                u32 cp = decode_utf8(p, end);
                if (cp == 0xFFFD) continue;

                // 1. 组合音标 (Combining Marks)
                if (is_combining(cp)) {
                    continue;
                }

                // 2. 零宽连字 (Zero Width Joiner)
                if (cp == 0x200D) {
                    if (p < end) {
                        decode_utf8(p, end);
                    }
                    continue;
                }

                // 3. 计宽
                total_width += static_cast<usize>(get_char_width(cp));
            }
            return total_width;
        }

    private:
        static bool is_combining(u32 cp) noexcept {
            return (cp >= 0x0300 && cp <= 0x036F) || 
                   (cp >= 0x1DC0 && cp <= 0x1DFF) || 
                   (cp >= 0x20D0 && cp <= 0x20FF) || 
                   (cp >= 0xFE20 && cp <= 0xFE2F);
        }

        static u32 decode_utf8(const unsigned char*& p, const unsigned char* end) noexcept {
            unsigned char c = *p++;
            if (c < 0x80) return c;
            if ((c & 0xE0) == 0xC0 && p < end) return ((c & 0x1F) << 6) | (*p++ & 0x3F);
            if ((c & 0xF0) == 0xE0 && p + 1 < end) {
                u32 res = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
                p += 2;
                return res;
            }
            if ((c & 0xF8) == 0xF0 && p + 2 < end) {
                u32 res = ((c & 0x07) << 18) | ((p[0] & 0x3F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                p += 3;
                return res;
            }
            return 0xFFFD;
        }

        static int get_char_width(u32 cp) noexcept {
            // 控制字符 (不计宽)
            if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;
            
            // 宽字符 (CJK、全角符号、绝大多数 Emoji)
            if ((cp >= 0x1100 && cp <= 0x115F) || 
                (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) || 
                (cp >= 0xAC00 && cp <= 0xD7A3) || 
                (cp >= 0xF900 && cp <= 0xFAFF) || 
                (cp >= 0xFE10 && cp <= 0xFE19) || 
                (cp >= 0xFF00 && cp <= 0xFF60) || 
                (cp >= 0xFFE0 && cp <= 0xFFE6) || 
                (cp >= 0x1F000 && cp <= 0x1F9FF) ||
                (cp >= 0x20000 && cp <= 0x3FFFD)) {
                return 2;
            }
            return 1;
        }
    };

    StringCalcInfo calc_string_dimensions(std::string_view sv, memory_resource* mem) {
        StringCalcInfo info(mem);
        auto split_lines = alib6::str::split(sv, '\n', mem);
        info.lines.reserve(split_lines.size());
        info.cols.reserve(split_lines.size());

        for (auto line : split_lines) {
            info.lines.push_back(line);
            usize w = Utf8Toolkit::get_display_width(line);
            info.cols.push_back(w);
            if (w > info.max_cols) {
                info.max_cols = w;
            }
        }
        return info;
    }

    TableConfig TableConfig::unicode_rounded() {
        TableConfig c;
        c.lborder = "│"; c.rborder = "│"; c.mid_column_sep = "│";
        c.line_sep = "─"; c.top_border = "─"; c.bottom_border = "─";
        c.top_left = "╭"; c.top_right = "╮"; c.top_joint = "┬";
        c.bottom_left = "╰"; c.bottom_right = "╯"; c.bottom_joint = "┴";
        c.lline_joint = "├"; c.rline_joint = "┤"; c.mid_line_joint = "┼";
        return c;
    }

    TableConfig TableConfig::unicode_box() {
        TableConfig c;
        c.lborder = "│"; c.rborder = "│"; c.mid_column_sep = "│";
        c.line_sep = "─"; c.top_border = "─"; c.bottom_border = "─";
        c.top_left = "┌"; c.top_right = "┐"; c.top_joint = "┬";
        c.bottom_left = "└"; c.bottom_right = "┘"; c.bottom_joint = "┴";
        c.lline_joint = "├"; c.rline_joint = "┤"; c.mid_line_joint = "┼";
        return c;
    }

    TableConfig TableConfig::double_line() {
        TableConfig c;
        c.lborder = "║"; c.rborder = "║"; c.mid_column_sep = "║";
        c.line_sep = "═"; c.top_border = "═"; c.bottom_border = "═";
        c.top_left = "╔"; c.top_right = "╗"; c.top_joint = "╦";
        c.bottom_left = "╚"; c.bottom_right = "╝"; c.bottom_joint = "╩";
        c.lline_joint = "╠"; c.rline_joint = "╣"; c.mid_line_joint = "╬";
        return c;
    }

    TableConfig TableConfig::modern_dot() {
        TableConfig c;
        c.lborder = "┋"; c.rborder = "┋"; c.mid_column_sep = "┊";
        c.line_sep = "┈"; c.top_border = "━"; c.bottom_border = "━";
        c.top_left = "┏"; c.top_right = "┓"; c.top_joint = "┳";
        c.bottom_left = "┗"; c.bottom_right = "┛"; c.bottom_joint = "┻";
        c.lline_joint = "┣"; c.rline_joint = "┫"; c.mid_line_joint = "╋";
        return c;
    }

    TableConfig TableConfig::markdown() {
        TableConfig c;
        c.enable_top_border = false;
        c.enable_bottom_border = false;
        c.enable_lborder = true;
        c.enable_rborder = true;
        c.enable_mid_sep = true;
        c.enable_line_sep = true;

        c.lborder = "|"; c.rborder = "|"; c.mid_column_sep = "|";
        c.line_sep = "-"; c.mid_line_joint = "|";
        c.lline_joint = "|"; c.rline_joint = "|";
        c.base_padding = 1;
        return c;
    }

    TableConfig TableConfig::minimal() {
        TableConfig c;
        c.enable_lborder = false;
        c.enable_rborder = false;
        c.enable_top_border = false;
        c.enable_bottom_border = false;
        c.enable_line_sep = true;

        c.mid_column_sep = " ";
        c.line_sep = "─";
        c.mid_line_joint = "─";
        c.lline_joint = "─";
        c.rline_joint = "─";
        c.base_padding = 2;
        return c;
    }

    TableConfig TableConfig::ascii_classic() {
        TableConfig c;
        c.lborder = "|"; c.rborder = "|"; c.mid_column_sep = "|";
        c.line_sep = "-"; c.top_border = "-"; c.bottom_border = "-";
        c.top_left = "+"; c.top_right = "+"; c.top_joint = "+";
        c.bottom_left = "+"; c.bottom_right = "+"; c.bottom_joint = "+";
        c.lline_joint = "+"; c.rline_joint = "+"; c.mid_line_joint = "+";
        return c;
    }

    void Table::swap_rows(u32 r1, u32 r2) {
        if (r1 == r2) return;
        u32 start_r = std::min(r1, r2);
        u32 end_r = std::max(r1, r2);

        if (!view_fixed) {
            top_row = std::min({top_row, static_cast<usize>(r1), static_cast<usize>(r2)});
            bottom_row = std::max({bottom_row, static_cast<usize>(r1), static_cast<usize>(r2)});
        }

        std::vector<data_t::node_type> nodes_a;
        std::vector<data_t::node_type> nodes_b;

        for (auto it = cells.lower_bound(Pos{.row = start_r, .col = 0});
             it != cells.end() && it->first.row <= end_r;) {
            if (it->first.row == r1) {
                auto node = cells.extract(it++);
                node.key().row = r2;
                nodes_a.push_back(std::move(node));
            } else if (it->first.row == r2) {
                auto node = cells.extract(it++);
                node.key().row = r1;
                nodes_b.push_back(std::move(node));
            } else {
                ++it;
            }
        }

        for (auto& n : nodes_a) cells.insert(std::move(n));
        for (auto& n : nodes_b) cells.insert(std::move(n));
    }

    void Table::swap_cols(u32 c1, u32 c2) {
        if (c1 == c2) return;
        if (!view_fixed) {
            left_col = std::min({left_col, static_cast<usize>(c1), static_cast<usize>(c2)});
            right_col = std::max({right_col, static_cast<usize>(c1), static_cast<usize>(c2)});
        }

        std::vector<data_t::node_type> nodes_a;
        std::vector<data_t::node_type> nodes_b;

        for (auto it = cells.begin(); it != cells.end();) {
            if (it->first.col == c1) {
                auto node = cells.extract(it++);
                node.key().col = c2;
                nodes_a.push_back(std::move(node));
            } else if (it->first.col == c2) {
                auto node = cells.extract(it++);
                node.key().col = c1;
                nodes_b.push_back(std::move(node));
            } else {
                ++it;
            }
        }

        for (auto& n : nodes_a) cells.insert(std::move(n));
        for (auto& n : nodes_b) cells.insert(std::move(n));
    }

} // namespace alib6::table
