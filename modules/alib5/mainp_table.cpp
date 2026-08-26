#include <alib5/compact/manip_table.h>
#include <alib5/compact/manip_table_impl.h>

using namespace alib5;
using namespace alib5::detail;

class Utf8Toolkit {
public:
    static size_t get_display_width(std::string_view sv) {
        size_t total_width = 0;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(sv.data());
        const unsigned char* end = p + sv.size();

        uint32_t last_cp = 0;

        while (p < end) {
            uint32_t cp = decode_utf8(p, end);
            if (cp == 0xFFFD) continue; // 忽略非法序列

            // 1. 处理组合字符 (Combining Marks)
            // 如果当前字符是组合字符，它附着在上一个字符上，不增加宽度
            if (is_combining(cp)) {
                continue; 
            }

            // 2. 处理零宽连字 (Zero Width Joiner)
            // 如果当前是 ZWJ，或者是连字后的随附部分，跳过计宽
            if (cp == 0x200D) {
                // 标记接下来的字符可能不计宽（简化处理：跳过下一个字符的宽度累加）
                if (p < end) {
                    decode_utf8(p, end); // 消耗掉被连结的下一个字符
                }
                continue;
            }

            // 3. 正常计宽
            total_width += get_char_width(cp);
            last_cp = cp;
        }
        return total_width;
    }

private:
    static bool is_combining(uint32_t cp) {
        // 常见的组合音标、变音符号范围
        return (cp >= 0x0300 && cp <= 0x036F) || 
               (cp >= 0x1DC0 && cp <= 0x1DFF) || 
               (cp >= 0x20D0 && cp <= 0x20FF) || 
               (cp >= 0xFE20 && cp <= 0xFE2F);
    }

    static uint32_t decode_utf8(const unsigned char*& p, const unsigned char* end) {
        unsigned char c = *p++;
        if (c < 0x80) return c;
        if ((c & 0xE0) == 0xC0 && p < end) return ((c & 0x1F) << 6) | (*p++ & 0x3F);
        if ((c & 0xF0) == 0xE0 && p + 1 < end) {
            uint32_t res = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
            p += 2; return res;
        }
        if ((c & 0xF8) == 0xF0 && p + 2 < end) {
            uint32_t res = ((c & 0x07) << 18) | ((p[0] & 0x3F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3; return res;
        }
        return 0xFFFD; // Unicode Replacement Character
    }

    static int get_char_width(uint32_t cp) {
        // 控制字符 (不计宽)
        if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;
        
        // 宽字符范围 (East Asian Wide / Fullwidth)
        // 涵盖中日韩汉字、谚文、全角符号、Emoji
        if ((cp >= 0x1100 && cp <= 0x115F) || 
            (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) || 
            (cp >= 0xAC00 && cp <= 0xD7A3) || 
            (cp >= 0xF900 && cp <= 0xFAFF) || 
            (cp >= 0xFE10 && cp <= 0xFE19) || 
            (cp >= 0xFF00 && cp <= 0xFF60) || 
            (cp >= 0xFFE0 && cp <= 0xFFE6) || 
            (cp >= 0x1F000 && cp <= 0x1F9FF) || // 绝大多数 Emoji
            (cp >= 0x20000 && cp <= 0x3FFFD)) {  // 扩展汉字
            return 2;
        }
        return 1; 
    }
};

StringCalcInfo alib5::_default_string_calc(std::string_view sv){
    StringCalcInfo info;
    info.lines = str::split(sv, '\n');
    info.max_cols = 0;
    
    for(auto s : info.lines){
        size_t display_w = Utf8Toolkit::get_display_width(s);
        info.cols.emplace_back(display_w);
        
        if(display_w > info.max_cols){
            info.max_cols = display_w;
        }
    }
    return info;
}

StreamedContext<LogFactory>&& log_table::self_forward(StreamedContext<LogFactory> && ctx){
    return _self_forward(std::move(ctx));
}
StreamedContext<detail::LogTableMockFactory>&& log_table::self_forward(StreamedContext<detail::LogTableMockFactory> && ctx){
    return _self_forward(std::move(ctx));
}

log_table::cfg log_table::unicode_rounded() {
    cfg c;
    // 纵向组件 (全部视觉宽度 1)
    c.lborder = "│";
    c.rborder = "│";
    c.mid_column_sep = "│";
    
    // 横向组件
    c.line_sep = "─";
    c.top_border = "─";
    c.bottom_border = "─";
    
    // 交汇点 (全部视觉宽度 1)
    c.top_left = "╭";      // 圆角风格
    c.top_right = "╮";
    c.top_joint = "┬";
    
    c.bottom_left = "╰";
    c.bottom_right = "╯";
    c.bottom_joint = "┴";
    
    c.lline_joint = "├";
    c.rline_joint = "┤";
    c.mid_line_joint = "┼";
    
    return c;
}

log_table::cfg log_table::minimal() {
    cfg c;
    // 1. 关闭侧边和底部物理开关
    c.enable_lborder = false;
    c.enable_rborder = false;
    c.enable_bottom_border = false;
    c.enable_line_sep = true; // 依然保留行分割线，类似 Markdown 效果

    // 2. 清除顶部组件 (设为空或空格，防止 + 出现)
    c.top_border = " ";
    c.top_left = " ";
    c.top_right = " ";
    c.top_joint = " ";

    // 3. 中间组件优化
    c.mid_column_sep = " ";   // 列之间用空格分隔，不显示竖线
    c.line_sep = "─";         // 行之间用细线
    c.mid_line_joint = "─";   // 关键：交汇点也要用横线填充，否则线会断开
    c.lline_joint = "─";      // 左侧不显示 +
    c.rline_joint = "─";      // 右侧不显示 +

    // 4. 底部组件清理 (虽然开关关了，但为了严谨也清空)
    c.bottom_border = " ";
    c.bottom_left = " ";
    c.bottom_right = " ";
    c.bottom_joint = " ";

    // 5. 间距补偿：没有竖线时，增加 padding 会更美观
    c.base_padding = 2; 
    
    return c;
}

log_table::cfg log_table::double_line(){
    cfg c;
    c.lborder = "║"; c.rborder = "║"; c.mid_column_sep = "║";
    c.line_sep = "═"; c.top_border = "═"; c.bottom_border = "═";
    
    c.top_left = "╔"; c.top_right = "╗"; c.top_joint = "╦";
    c.bottom_left = "╚"; c.bottom_right = "╝"; c.bottom_joint = "╩";
    
    c.lline_joint = "╠"; c.rline_joint = "╣"; c.mid_line_joint = "╬";
    return c;
}

log_table::cfg log_table::modern_dot(){
    cfg c;
    c.lborder = "┋"; c.rborder = "┋"; c.mid_column_sep = "┊";
    c.line_sep = "┈"; c.top_border = "━"; c.bottom_border = "━";
    
    c.top_left = "┏"; c.top_right = "┓"; c.top_joint = "┳";
    c.bottom_left = "┗"; c.bottom_right = "┛"; c.bottom_joint = "┻";
    
    c.lline_joint = "┣"; c.rline_joint = "┫"; c.mid_line_joint = "╋";
    return c;
}