#ifndef ALIB5_COMPACT_MANIP_TABLE
#define ALIB5_COMPACT_MANIP_TABLE
#include <alib5/compact/manip_table.h>

namespace alib5{
    using namespace detail;

    template<class Lg>
    inline StreamedContext<Lg>&& log_table::_self_forward(StreamedContext<Lg> && ctx){
        data_t data(ctx.factory.get_msg_str_alloc());
        Operator op(data);
        op.factory.emplace(ctx.factory);
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
        /// 这里传入的大小都是逻辑大小,并且假设line_sep等等逻辑大小就是1
        auto gen_data = [](std::string & buf,std::string_view base,size_t logical_size){
            size_t current = buf.size() / base.size();
            if(current < logical_size){
                buf.reserve(buf.size() + (logical_size - current) * base.size());
                for(size_t i = current;i <= logical_size;++i){
                    buf.append(base);
                }
            }
            return std::string_view(buf).substr(0,logical_size * base.size());
        };

        auto gen_line_sep = [&line_sep,&gen_data,this](size_t sz){
            return gen_data(line_sep, config.line_sep, sz);
        };
        auto gen_top_border = [&top_border,&gen_data,this](size_t sz){
            return gen_data(top_border,config.top_border,sz);
        };
        auto gen_bottom_border = [&bottom_border,&gen_data,this](size_t sz){
            return gen_data(bottom_border,config.bottom_border,sz);
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

        /// 当前大小,之后获取大小都是以这个为标准
        size_t cache_size = ctx.cache_str.size();
        auto push = [&](auto && val,size_t sz = 0){
            using T = std::decay_t<decltype(val)>;
            if constexpr(std::is_convertible_v<T,std::string_view>){
                auto str = std::string_view(val);
                ctx.cache_str.append(str);

                if(sz)cache_size += sz;
                else cache_size += str.size();
            }else{
                ctx.cache_str.push_back((char)val);
                cache_size += 1;
            }
        };
        auto get_size = [&]{
            return cache_size;
        };

        /// 生成top_border
        if(config.enable_lborder)push(config.top_left,1);
        for(size_t i = 0;i < col_lengths.size();++i){
            if(col_lengths[i] == 0)continue;
            push(
                gen_top_border(col_lengths[i] + 2 * config.base_padding),
                col_lengths[i] + 2 * config.base_padding
            );
            if(config.enable_mid_sep && i + 1 != col_lengths.size()){
                push(config.top_joint,1);
            }
        }
        if(config.enable_rborder)push(config.top_right,1);
        push('\n');
        
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
                    push('\n');                      
                }
                size_t old_col = cont[0]->first.col;

                /// 开始写入,加入lborder
                if(config.enable_lborder)push(config.lborder,1);

                auto end_iter = *(cont.end() - 1);
                size_t oddy_index = get_size();

                auto fill_blanks = [&](size_t to){
                    // 跳过没有信息的行
                    for(size_t skip_col = old_col;skip_col < to;++skip_col){
                        size_t len = col_lengths[access_col(skip_col)];
                        // 这个策略是skip策略
                        if(!len)continue;
                        // 因为还涉及边框问题,所以这里不使用统一写入而是分段写入
                        // 这里的加1是分割符,后面会定制化
                        push(
                            gen_space(len + 2 * config.base_padding)
                        );
                        if(config.enable_mid_sep){
                            push(config.mid_column_sep,1);
                            if(!generated_mid_sep){
                                mid_sep_positions.push_back(get_size() - oddy_index);
                            }
                        }
                    }
                };

                for(auto it : cont){
                    Pos pos = it->first;
                    Item & item = it->second;

                    fill_blanks(pos.col);
                    old_col = pos.col + 1;

                    // 现在到了pos.col的位置,写入对应的lines
                    size_t current_col_len = col_lengths[access_col(pos.col)];
                    // 写入padding
                    push(gen_space(config.base_padding));

                    size_t i_beg = 0,i_end = item.info.lines.size();
                    switch(item.row_align.value_or(config.row_align)){
                    case RowAlign::Bottom:
                        i_end = row_lengths[access_row(pos.row)];
                        i_beg = i_end - item.info.lines.size();
                        break;
                    case RowAlign::Top:
                        break;
                    case RowAlign::Center:{
                        size_t off = row_lengths[access_row(pos.row)] - item.info.lines.size();
                        i_beg = off/2;
                        i_end = i_beg + item.info.lines.size();
                    }
                        break;
                    }

                    if(i >= i_beg && i < i_end){
                        size_t write_index = item.write_index++;
                        std::string_view line = item.info.lines[write_index];
                        size_t line_size = item.info.cols[write_index];

                        size_t space_bef = 0,space_aft = current_col_len - line_size;
                        switch(item.col_align.value_or(config.col_align)){
                        case ColAlign::Center:
                            space_bef = space_aft/2;
                            space_aft = space_aft - space_bef;
                            break;
                        case ColAlign::Left:
                            break;
                        case ColAlign::Right:
                            space_bef = space_aft;
                            space_aft = 0;
                            break;
                        }

                        // 前空格
                        if(space_bef)push(gen_space(space_bef));

                        // 这里写入了实际数据,因此需要push_tags以及restore_tags
                        // 由于对于局部的context是覆盖制,因此换行需要把之前的tags全部搞进来
                        size_t real_sz = ctx.cache_str.size();
                        for(LogCustomTag tag : item.context.tags){
                            // 因为write_index本身是线性增加的,因此次item中可以存在保存的信息
                            size_t line_revelant_pos = 
                                (/*在字符串中的绝对位置*/tag.get() <= item.cached_index) ?
                                0 : tag.get() - item.cached_index;
                            // 在最后给你全部加进去
                            // 比如 op << Red << "Hello\n" << "World" << Reset
                            // 这里就需要显式加入reset
                            line_revelant_pos = std::min(line_revelant_pos,line_size);
                            tag.set(line_revelant_pos + real_sz);
                            ctx.tags.emplace_back(tag);
                        }
                        item.cached_index += line.size() + 1; // 这里可以放心+1,因为没有\n的是最后一行,最后一行还管你cache_index?

                        if(!line.empty()) [[likely]]{
                            if(line.back() == '\r'){
                                push(line.substr(0,line.size() - 1),line_size - 1);
                                // 假装我看到一个空格
                                push(' ');
                            }else{
                                push(line,line_size);
                            }
                        }
                        
                        real_sz = ctx.cache_str.size();
                        for(LogCustomTag tag : restore_tags){
                            tag.set(real_sz);
                            ctx.tags.emplace_back(tag);
                        }
                        // 前空格
                        if(space_aft)push(gen_space(space_aft));
                    }else{ // 写入最大空格,处理也是这样的
                        push(gen_space(current_col_len));
                    }
                    // 写入完成padding
                    push(gen_space(config.base_padding));
                    if(it != end_iter && config.enable_mid_sep){
                        push(config.mid_column_sep,1);
                    }else if(config.enable_rborder) push(config.rborder,1);

                    if(!generated_mid_sep){
                        mid_sep_positions.push_back(get_size() - oddy_index);
                    }
                }

                /// 补全末尾信息
                fill_blanks(op.right + 1);
                generated_mid_sep = true;
            }
            // 写入完毕
            if(i + 1 != iterators.size()){
                // 最后一行不换行

                push('\n');
                if(config.enable_line_sep){
                    size_t bef = 0;
                    if(config.enable_lborder)push(config.lline_joint,1);
                    if(config.enable_mid_sep){
                        for(size_t mid_i = 0;mid_i < mid_sep_positions.size();++mid_i){
                            size_t & cur = mid_sep_positions[mid_i];
                            size_t lsep_size = cur > bef ? cur - bef - 1 : 0;
                            push(gen_line_sep(lsep_size),lsep_size);
                            bef = cur;

                            /// @todo 最后一个用的是border joint
                            if(mid_i+1 != mid_sep_positions.size()){
                                push(config.mid_line_joint,1);
                            }
                        }
                    }else{
                        for(size_t acci = 0;acci < col_lengths.size();++acci){
                            if(col_lengths[acci] == 0)continue;
                            size_t push_sz = col_lengths[acci] - (int)(acci+1 == col_lengths.size() ? 1 : 0);

                            push(
                                gen_line_sep(push_sz + 2 * config.base_padding),
                                push_sz + 2 * config.base_padding
                            );
                        }
                    }
                    
                    if(config.enable_rborder) push(config.rline_joint,1);
                    else push(config.line_sep,1);
                    push('\n');
                }
            }
        }

        /// 生成bottom border
        if(config.enable_bottom_border){
            push('\n');
            if(config.enable_lborder)push(config.bottom_left,1);
            for(size_t i = 0;i < col_lengths.size();++i){
                if(col_lengths[i] == 0)continue;
                push(
                    gen_bottom_border(col_lengths[i] + 2 * config.base_padding),
                    col_lengths[i] + 2 * config.base_padding
                );
                if(config.enable_mid_sep && i + 1 != col_lengths.size()){
                    push(config.bottom_joint,1);
                }
            }
            if(config.enable_rborder)push(config.bottom_right,1);
        }

        if(config.wrap_new_line)push('\n');
        return std::move(ctx);
    }
};

#endif