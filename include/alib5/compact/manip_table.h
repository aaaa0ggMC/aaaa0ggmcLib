#ifndef ALIB5_LOG_MANIP_TABLE
#define ALIB5_LOG_MANIP_TABLE
#include <alib5/autil.h>
#include <alib5/log/manipulator.h>
#include <alib5/log/streamed_context.h>
#include <alib5/log/kernel.h>
#include <map>

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
            size_t cached_index { 0 };

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
            using ctx_t = StreamedContext<LogFactory> &;
            ctx_t context;
            data_t & data;
            detail::LogTableMockFactory factory;

            size_t top { std::variant_npos };
            size_t left { std::variant_npos };
            size_t right { 0 };
            size_t bottom { 0 };

            Operator(ctx_t context,data_t & data)
            :context(context)
            ,data(data)
            ,factory(context.factory){}
            
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
                        auto [it, _] = op.data.try_emplace(
                            Pos{.row = row, .col = col}, 
                            op.factory
                        );
                        return it->second;
                    }

                    template<class T>
                    auto& operator<<(T && val){
                        return (get() << std::forward<T>(val));
                    }
                };

                Col operator[](uint32_t i){
                    if(i < op.left)op.left = i;
                    if(i > op.right)op.right = i;
                    return Col(row,i,op);
                }
            };

            Row operator[](uint32_t i){
                if(i < top)top = i;
                if(i > bottom)bottom = i;
                return Row(i,*this);
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
            char mid_column_sep { '|' };
            /// 单独的行分割
            bool enable_line_sep { true };
            char line_sep { '-' }; ///< 每个表左侧元素的分割符号
            char mid_line_joint { '+' }; ///< mid_sep与line_sep相交时的字符
            /// 左右侧border
            bool enable_lborder { true };
            char lborder { '|' };
            char lline_joint { '+' };
            
            bool enable_rborder { true };
            char rborder { '|' };
            char rline_joint { '+' };
            /// 顶部border
            char top_border { '-' };
            char top_joint { '+' };
            char top_left { '+' };
            char top_right { '+' };
            /// 底部border
            bool enable_bottom_border { true };
            char bottom_border { '-' };
            char bottom_joint { '+' };
            char bottom_left { '+' };
            char bottom_right { '+' };

            /// 基础padding
            size_t base_padding { 1 };

            /// 最后换行
            bool wrap_new_line { false };

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

        StreamedContext<LogFactory>&& self_forward(StreamedContext<LogFactory> && ctx);
    };

}

#endif