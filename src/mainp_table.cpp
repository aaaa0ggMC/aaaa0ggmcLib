#include <alib5/compact/manip_table.h>

using namespace alib5;
using namespace alib5::detail;

StringCalcInfo alib5::_default_string_calc(std::string_view sv){
    StringCalcInfo info;
    info.lines = str::split(sv, '\n');
    info.max_cols = 0;
    for(auto s : info.lines){
        info.cols.emplace_back(s.size());
        if(s.size() > info.max_cols)info.max_cols = s.size();
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
    data_t data(ctx.factory.get_msg_str_alloc());
    Operator op(ctx,data);
    func(op);

    // 空表返回,这个很重要,后面的代码假设op.data不为空
    if(op.data.empty())return std::move(ctx);

    std::string expanding = "              ";
    std::string line_sep = "";
    std::string top_border = "";
    std::string bottom_border = "";
    auto gen_space= [&expanding](size_t sz){
        if(sz >= expanding.size()){
            expanding.resize(sz + 1,' ');
        }
        return std::string_view(expanding).substr(0,sz);
    };
    auto gen_line_sep = [&line_sep,this](size_t sz){
        if(sz >= line_sep.size()){
            line_sep.resize(sz + 1,config.line_sep);
        }
        return std::string_view(line_sep).substr(0,sz);
    };
    auto gen_top_border = [&top_border,this](size_t sz){
        if(sz >= top_border.size()){
            top_border.resize(sz + 1,config.top_border);
        }
        return std::string_view(top_border).substr(0,sz);
    };
    auto gen_bottom_border = [&bottom_border,this](size_t sz){
        if(sz >= bottom_border.size()){
            bottom_border.resize(sz + 1,config.bottom_border);
        }
        return std::string_view(bottom_border).substr(0,sz);
    };
    auto access_row = [&op](size_t row) -> size_t {
        return row - op.top;
    };
    auto access_col = [&op](size_t col) -> size_t{
        return col - op.left;
    };
    
    /// 分配大小计算格子
    if(op.right < op.left || op.bottom < op.top)return std::move(ctx); // 空表
    std::vector<size_t> col_lengths(access_col(op.right) + 1,0);
    std::vector<size_t> row_lengths(access_row(op.bottom) + 1,0);
    std::vector<
        std::vector<typename data_t::iterator>
    > iterators;
    
    /// 第一次遍历,获取边界
    {
        size_t old_row = op.data.empty() ? std::variant_npos : op.data.begin()->first.row;
        
        auto * focus = &iterators.emplace_back();
        for(auto it = op.data.begin();it != op.data.end();++it){
            auto & [pos,data] = *it;
            if(old_row != pos.row){
                focus = &iterators.emplace_back();
                old_row = pos.row;
            }
            focus->emplace_back(it);

            auto & d_col = col_lengths[access_col(pos.col)];
            auto & d_row = row_lengths[access_row(pos.row)];
            data.info = calc(data.view());
            if(data.info.max_cols > d_col){
                d_col = data.info.max_cols; 
            }
            if(data.info.lines.size() > d_row){
                d_row = data.info.lines.size();
            }
        }
    }

    /// 生成top_border
    if(config.enable_lborder)ctx.cache_str.push_back(config.top_left);
    for(size_t i = 0;i < col_lengths.size();++i){
        if(col_lengths[i] == 0)continue;
        ctx.cache_str.append(
            gen_top_border(col_lengths[i] + 2 * config.base_padding)
        );
        if(config.enable_mid_sep && i + 1 != col_lengths.size())ctx.cache_str.push_back(config.top_joint);
    }
    if(config.enable_rborder)ctx.cache_str.push_back(config.top_right);
    ctx.cache_str.push_back('\n');
    
    /// 第二次遍历,生成内容
    std::vector<size_t> mid_sep_positions;
    for(size_t i = 0;i < iterators.size();++i){
        auto & cont = iterators[i];
        // 每一行都是有效行,因此添加换行
        size_t row_index = cont[0]->first.row;

        mid_sep_positions.clear();
        bool generated_mid_sep = false;
        for(size_t i = 0;i < row_lengths[access_row(row_index)];++i){  
            if(i != 0){
                // 写入rborder
                ctx.cache_str.push_back('\n');                      
            }
            size_t old_col = cont[0]->first.col;

            /// 开始写入,加入lborder
            if(config.enable_lborder)ctx.cache_str.push_back(config.lborder);

            auto end_iter = *(cont.end() - 1);
            size_t oddy_index = ctx.cache_str.size();
            for(auto it : cont){
                Pos pos = it->first;
                Item & item = it->second;

                // 跳过没有信息的行
                for(size_t skip_col = old_col;skip_col < pos.col;++skip_col){
                    size_t len = col_lengths[access_col(skip_col)];
                    // 这个策略是skip策略
                    if(!len)continue;
                    // 因为还涉及边框问题,所以这里不使用统一写入而是分段写入
                    // 这里的加1是分割符,后面会定制化
                    ctx.cache_str.append(
                        gen_space(len + 2 * config.base_padding)
                    );
                    if(config.enable_mid_sep){
                        ctx.cache_str.push_back(config.mid_column_sep);
                        if(!generated_mid_sep)mid_sep_positions.push_back(ctx.cache_str.size() - oddy_index);
                    }
                }
                old_col = pos.col + 1;

                // 现在到了pos.col的位置,写入对应的lines
                size_t current_col_len = col_lengths[access_col(pos.col)];
                // 写入padding
                ctx.cache_str.append(gen_space(config.base_padding));
                if(i < item.info.lines.size()){
                    size_t write_index = i;
                    std::string_view line = item.info.lines[write_index];
                    // 这里写入了实际数据,因此需要push_tags以及restore_tags
                    // 由于对于局部的context是覆盖制,因此换行需要把之前的tags全部搞进来
                    size_t sz = ctx.cache_str.size();
                    for(LogCustomTag tag : item.context.tags){
                        // 因为write_index本身是线性增加的,因此次item中可以存在保存的信息
                        size_t line_revelant_pos = 
                                (/*在字符串中的绝对位置*/tag.get() <= item.cached_index) ?
                                0 : tag.get() - item.cached_index;
                            // 在最后给你全部加进去
                            // 比如 op << Red << "Hello\n" << "World" << Reset
                            // 这里就需要显式加入reset
                            line_revelant_pos = std::min(line_revelant_pos,line.size());
                            tag.set(line_revelant_pos + sz);
                            ctx.tags.emplace_back(tag);
                        }
                        item.cached_index += line.size() + 1; // 这里可以放心+1,因为没有\n的是最后一行,最后一行还管你cache_index?

                        if(!line.empty()) [[likely]]{
                            if(line.back() == '\r'){
                                ctx.cache_str.append(line.substr(0,line.size() - 1));
                                // 假装我看到一个空格
                                ctx.cache_str.push_back(' ');
                            }else{
                                ctx.cache_str.append(line);
                            }
                        }
                        
                        sz = ctx.cache_str.size();
                        for(LogCustomTag tag : restore_tags){
                        tag.set(sz);
                        ctx.tags.emplace_back(tag);
                    }
                    ctx.cache_str.append(gen_space(current_col_len - line.size()));
                }else{ // 写入最大空格,处理也是这样的
                    ctx.cache_str.append(gen_space(current_col_len));
                }
                // 写入完成padding
                ctx.cache_str.append(gen_space(config.base_padding));
                if(it != end_iter && config.enable_mid_sep){
                    ctx.cache_str.push_back(config.mid_column_sep);
                }else if(config.enable_rborder) ctx.cache_str.push_back(config.rborder);

                if(!generated_mid_sep)mid_sep_positions.push_back(ctx.cache_str.size() - oddy_index);
            }
            generated_mid_sep = true;
        }
        // 写入完毕
        if(i + 1 != iterators.size()){
            // 最后一行不换行

            ctx.cache_str.push_back('\n');
            if(config.enable_line_sep){
                size_t bef = 0;
                if(config.enable_lborder)ctx.cache_str.push_back(config.lline_joint);
                if(config.enable_mid_sep){
                    for(size_t mid_i = 0;mid_i < mid_sep_positions.size();++mid_i){
                        size_t & cur = mid_sep_positions[mid_i];
                        ctx.cache_str.append(gen_line_sep(cur > bef ? cur - bef - 1 : 0));
                        bef = cur;

                        /// @todo 最后一个用的是border joint
                        if(mid_i+1 != mid_sep_positions.size())ctx.cache_str.push_back(config.mid_line_joint);
                    }
                }else{
                    for(size_t acci = 0;acci < col_lengths.size();++acci){
                        if(col_lengths[acci] == 0)continue;
                        size_t push_sz = col_lengths[acci] - (int)(acci+1 == col_lengths.size() ? 1 : 0);

                        ctx.cache_str.append(
                            gen_line_sep(push_sz + 2 * config.base_padding)
                        );
                    }
                }
                
                if(config.enable_rborder) ctx.cache_str.push_back(config.rline_joint);
                else ctx.cache_str.push_back(config.line_sep);
                ctx.cache_str.push_back('\n');
            }
        }
    }

    /// 生成bottom border
    if(config.enable_bottom_border){
        ctx.cache_str.push_back('\n');
        if(config.enable_lborder)ctx.cache_str.push_back(config.bottom_left);
        for(size_t i = 0;i < col_lengths.size();++i){
            if(col_lengths[i] == 0)continue;
            ctx.cache_str.append(
                gen_bottom_border(col_lengths[i] + 2 * config.base_padding)
            );
            if(config.enable_mid_sep && i + 1 != col_lengths.size())ctx.cache_str.push_back(config.bottom_joint);
        }
        if(config.enable_rborder)ctx.cache_str.push_back(config.bottom_right);
    }

    if(config.wrap_new_line)ctx.cache_str.push_back('\n');
    return std::move(ctx);
}