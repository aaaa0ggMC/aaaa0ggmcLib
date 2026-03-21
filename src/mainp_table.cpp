#include <alib5/compact/manip_table.h>
#include <alib5/compact/manip_table_impl.h>

using namespace alib5;
using namespace alib5::detail;

/// from @Gemini
inline static size_t get_utf8_display_width(std::string_view sv) {
    size_t width = 0;
    for (size_t i = 0; i < sv.size(); ) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        
        if (c < 0x80) { 
            // ASCII 字符
            // 特殊处理 \r，由于你之前的逻辑将其看作 1 宽的空格，这里保持一致
            // 如果你打算完全忽略 \r 宽度，这里返回 0
            if (c != '\r') width += 1;
            else width += 1; // 对应你之前的“换成空格”逻辑
            i += 1;
        } else if ((c & 0xE0) == 0xC0) { 
            // 2 字节字符 (如希腊字母、阿拉伯文) - 通常占 1 宽
            width += 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) { 
            // 3 字节字符 (大部分中文、日文、韩文) - 占 2 宽
            width += 2;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) { 
            // 4 字节字符 (Emoji、极少数生僻字) - 占 2 宽
            width += 2;
            i += 4;
        } else {
            // 非法 UTF-8 编码，安全起见跳过 1 字节
            i += 1;
        }
    }
    return width;
}

StringCalcInfo alib5::_default_string_calc(std::string_view sv){
    StringCalcInfo info;
    info.lines = str::split(sv, '\n');
    info.max_cols = 0;
    
    for(auto s : info.lines){
        size_t display_w = get_utf8_display_width(s);
        info.cols.emplace_back(display_w);
        
        if(display_w > info.max_cols){
            info.max_cols = display_w;
        }
    }
    return info;
}
log_table::log_table(
    OperatorFn fn,
    StringCalcFn calc,
    const cfg& c,
    std::pmr::memory_resource * allocator
):func(fn)
,config(c)
,allocator(allocator)
,calc(calc)
,restore_tags(allocator){}

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