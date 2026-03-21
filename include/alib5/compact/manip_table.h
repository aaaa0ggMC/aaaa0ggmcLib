/**
 * @file manip_table.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一个简单的表格系统,用于logger显示表格数据,支持多行,颜色,嵌套,对齐等功能
 * @version 5.0
 * @date 2026-03-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_LOG_MANIP_TABLE
#define ALIB5_LOG_MANIP_TABLE
#include <alib5/autil.h>
#include <alib5/log/manipulator.h>
#include <alib5/log/streamed_context.h>
#include <alib5/log/kernel.h>
#include <map>
#include <cstdlib>

namespace alib5{
    /// 计算一个字符串所占用的行/列大小
    struct StringCalcInfo{
        /// 每一行,建议以'\n'进行切分
        std::vector<std::string_view> lines;
        /// 每一列
        std::vector<size_t> cols;
        /// cols中的最大项
        size_t max_cols;
    };
    /// 默认的计算字符串的方式,目前支持英文
    StringCalcInfo ALIB5_API _default_string_calc(std::string_view sv);

    /// 对齐
    enum class ColAlign{
        Left,
        Center,
        Right
    };
    enum class RowAlign{
        Top,
        Center,
        Bottom
    }; 

    struct log_table;
    namespace detail{
        /// Mock LogFactory用于复用context的格式化能力
        struct LogTableMockFactory{
            LogFactory & fac;

            LogTableMockFactory(LogFactory & real)
            :fac(real){}

            auto& get_msg_str_alloc(){
                return fac.logger.msg_str_alloc;
            }
            auto& get_tag_alloc(){
                return fac.logger.tag_alloc;
            }
            auto& get_msg_config(){
                return fac.cfg.msg;
            }

            bool log_pmr(int level,std::pmr::string & pmr_data,const LogMsgConfig & mcfg,std::pmr::vector<LogCustomTag> & tags){
                return false;
            }
        };

        struct Item{
            // 十分不建议直接使用context
            StreamedContext<detail::LogTableMockFactory> context;
            StringCalcInfo info;
            bool calced = false;

            std::optional<ColAlign> col_align;
            std::optional<RowAlign> row_align;

            Item(detail::LogTableMockFactory & fac)
            :context(0,fac){}

            template<class T>
            Item& operator<<(T && val){
                // 复位context
                // 几乎没有人会在表格里面endlog吧,太不合理了
                if(context.context_used)[[unlikely]] context.context_used = false;
                std::move(context) << std::forward<T>(val);
                return *this;
            }

            std::string_view view() const { return context.cache_str; }
        };

        /// 位置信息
        union Pos {
            uint64_t composed { 0 };
            struct {
                uint32_t row;
                uint32_t col;
            };

            auto operator<=>(const Pos & a){
                return std::tie(row,col) <=> std::tie(a.row,a.col);
            }
        };
        /// 位置比较函数
        struct PosCompare{
            bool operator()(const Pos & a,const Pos b) const {
                return std::tie(a.row, a.col) < std::tie(b.row, b.col);
            }
        };
        /// 数据类型
        using data_t = std::pmr::map<
            Pos,
            detail::Item,
            PosCompare
        >;

        struct Operator{
        private:
            friend class alib5::log_table;

            data_t & data;
            std::optional<detail::LogTableMockFactory> factory;
            size_t top { std::variant_npos };
            size_t left { std::variant_npos };
            size_t right { 0 };
            size_t bottom { 0 };
            bool view_fixed { false };
            std::optional<Item> default_value;

        public:
            Operator(data_t & data)
            :data(data){}
            
            struct Row{
            private:
                Operator & op;
                uint32_t row;
            public:
                Row(uint32_t r,Operator& op)
                :row(r)
                ,op(op){}

                struct Col{
                private:
                    Operator & op;
                    uint32_t row;
                    uint32_t col;
                public:

                    Col(uint32_t row,uint32_t col,Operator & op)
                    :row(row),col(col),op(op){}
                
                    detail::Item& get(){
                        if(row == std::numeric_limits<uint32_t>::max() ||
                           col == std::numeric_limits<uint32_t>::max()
                        ){
                            return op.get_placeholder_item();
                        }else{
                            auto [it, _] = op.data.try_emplace(
                                Pos{.row = row, .col = col}, 
                                *op.factory
                            );
                            return it->second;
                        }
                    }

                    auto& col_align(ColAlign align){
                        get().col_align.emplace(align);
                        return *this;
                    }

                    auto& row_align(RowAlign align){
                        get().row_align.emplace(align);
                        return *this;
                    }
                    
                    void clear(){
                        auto & v = get();
                        v.context.tags.clear();
                        v.context.cache_str.clear();
                        v.context.fmt_tmp = true;
                        v.context.fmt_str = "";
                    }

                    template<IsStringLike T>
                    auto& operator=(T && val){
                        clear();
                        get().context.cache_str = std::string_view(val);
                        return *this;
                    }

                    template<class T>
                    auto& operator<<(T && val){
                        return (get() << std::forward<T>(val));
                    }
                };

                Col operator[](uint32_t i){
                    if(!op.view_fixed){
                        if(i < op.left)op.left = i;
                        if(i > op.right)op.right = i;
                    }
                    return Col(row,i,op);
                }
            };

            Row operator[](uint32_t i){
                if(!view_fixed){
                    if(i < top)top = i;
                    if(i > bottom)bottom = i;
                }
                return Row(i,*this);
            }

            void set_view(uint32_t row,uint32_t col,uint32_t row_l,uint32_t col_l){
                top = row;
                left = col;
                right = col + col_l;
                bottom = row + row_l;

                view_fixed = true;
            }

            void swap_rows(uint32_t row_a, uint32_t row_b){
                if(row_a == row_b)return;
                uint32_t start_row = std::min(row_a, row_b);
                uint32_t end_row = std::max(row_a, row_b);

                if(!view_fixed){
                    top = std::min({(uint32_t)top, row_a, row_b});
                    bottom = std::max({(uint32_t)bottom, row_a, row_b});
                }

                std::vector<data_t::node_type> nodes_a;
                std::vector<data_t::node_type> nodes_b;
                for(auto it = data.lower_bound(Pos{.row = start_row, .col = 0}); 
                    it != data.end() && it->first.row <= end_row;){
                    if(it->first.row == row_a){
                        auto node = data.extract(it++);
                        node.key().row = row_b;
                        nodes_a.push_back(std::move(node));
                    }else if(it->first.row == row_b) {
                        auto node = data.extract(it++);
                        node.key().row = row_a;
                        nodes_b.push_back(std::move(node));
                    }else{
                        ++it;
                    }
                }
                for(auto& n : nodes_a) data.insert(std::move(n));
                for(auto& n : nodes_b) data.insert(std::move(n));
            }

            Row::Col placeholder(){
                return Row::Col(std::numeric_limits<uint32_t>::max(),0,*this);
            }

            Item& get_placeholder_item(){
                // factory会在lambda中构建
                if(!default_value){
                    default_value.emplace(*factory);
                }
                return *default_value;
            }
        };
    }


    using StringCalcFn = std::function<StringCalcInfo(std::string_view)>;
    using OperatorFn = std::function<void(detail::Operator&)>;

    /// 核心出装
    struct ALIB5_API log_table{
        /// 配置
        struct cfg{
            /// 每个表的元素中间的分割符号
            bool enable_mid_sep { true };
            std::string mid_column_sep { '|' };
            /// 单独的行分割
            bool enable_line_sep { true };
            std::string line_sep { '-' }; ///< 每个表左侧元素的分割符号
            std::string mid_line_joint { '+' }; ///< mid_sep与line_sep相交时的字符
            /// 左右侧border
            bool enable_lborder { true };
            std::string lborder { '|' };
            std::string lline_joint { '+' };
            
            bool enable_rborder { true };
            std::string rborder { '|' };
            std::string rline_joint { '+' };
            /// 顶部border
            std::string top_border { '-' };
            std::string top_joint { '+' };
            std::string top_left { '+' };
            std::string top_right { '+' };
            /// 底部border
            bool enable_bottom_border { true };
            std::string bottom_border { '-' };
            std::string bottom_joint { '+' };
            std::string bottom_left { '+' };
            std::string bottom_right { '+' };
            /// 基础padding
            size_t base_padding { 1 };
            /// 最后换行
            bool wrap_new_line { false };
            ColAlign col_align { ColAlign::Left };
            RowAlign row_align { RowAlign::Top };

            cfg(){}
        };

        OperatorFn func;
        StringCalcFn calc;
        cfg config; 
        std::pmr::memory_resource * allocator;
        std::pmr::vector<LogCustomTag> restore_tags;

        log_table(
            OperatorFn fn,
            StringCalcFn calc = _default_string_calc,
            const cfg& c = cfg(),
            std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE
        );

        /// 支持标准版本以及Mock版本
        StreamedContext<LogFactory>&& self_forward(StreamedContext<LogFactory> && ctx);
        StreamedContext<detail::LogTableMockFactory>&& self_forward(StreamedContext<detail::LogTableMockFactory> && ctx);

        /// 具体实现
        template<class Lg>
        StreamedContext<Lg>&& _self_forward(StreamedContext<Lg> && ctx);
    
        static ALIB5_API cfg unicode_rounded();
        static ALIB5_API cfg minimal();
        static ALIB5_API cfg double_line();
        static ALIB5_API cfg modern_dot();
    };

}

#endif