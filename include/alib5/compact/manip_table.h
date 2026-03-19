#ifndef ALIB5_LOG_MANIP_TABLE
#define ALIB5_LOG_MANIP_TABLE
#include <alib5/autil.h>
#include <alib5/log/manipulator.h>
#include <alib5/log/streamed_context.h>
#include <alib5/log/kernel.h>
#include <map>

namespace alib5{
    inline size_t _default_string_calc(std::string_view sv){
        return sv.length();
    }

    template<class Fn,class StringLengthCalc = decltype(&_default_string_calc)>
    struct log_table{
        struct cfg{

            cfg(){

            }
        };

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

            bool log_pmr(int level,std::pmr::string & pmr_data,const LogMsgConfig & mcfg,std::pmr::vector<LogCustomTag> & tags){}
        };

        struct Item{
            // 十分不建议直接使用context
            StreamedContext<LogTableMockFactory> context;
            size_t len { 0 };

            Item(LogTableMockFactory & fac)
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

            Item(LogFactory & factory)
            :context(0,factory){}
        };

        union Pos {
            uint64_t composed { 0 };
            struct {
                uint32_t row;
                uint32_t col;
            };
        };

        struct PosCompare{
            bool operator()(const Pos & a,const Pos b) const {
                return std::tie(a.row, a.col) < std::tie(b.row, b.col);
            }
        };

        using data_t = std::pmr::map<
            Pos,
            Item,
            PosCompare
        >;
        struct Operator{
            using ctx_t = StreamedContext<LogFactory> &;
            ctx_t context;
            data_t & data;
            LogTableMockFactory factory;

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
                
                    Item& get(){
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

        Fn func;
        StringLengthCalc calc;
        cfg config; 
        std::pmr::memory_resource * allocator;

        log_table(
            Fn && fn,
            StringLengthCalc && calc = _default_string_calc,
            const cfg& c = cfg(),
            std::pmr::memory_resource * allocator = ALIB5_DEFAULT_MEMORY_RESOURCE
        ):func(std::forward<Fn>(fn))
        ,config(c)
        ,allocator(allocator)
        ,calc(calc){}

        StreamedContext<LogFactory>&& self_forward(StreamedContext<LogFactory> && ctx){
            data_t data(ctx.factory.get_msg_str_alloc());
            Operator op(ctx,data);
            func(op);

            std::cout << op.left << " " << op.top << " " << op.right << " " << op.bottom << " " << std::endl; 

            /// 分配大小计算格子
            if(op.right < op.left || op.bottom < op.top)return std::move(ctx); // 空表
            std::vector<size_t> lengths(op.right - op.left + 1,0);
            std::vector<bool> row_has_data(op.bottom - op.top + 1,false);
            std::string expanding = "              ";
            auto gen_space= [&](size_t sz){
                if(sz >= expanding.size()){
                    expanding.resize(sz + 1,' ');
                }
                return expanding.substr(0,sz);
            };

            for(auto & [pos,data] : op.data){
                auto & p = lengths[pos.col - op.left];
                data.len = calc(data.view());
                if(data.len >= p){
                    p = data.len; 
                }
                row_has_data[pos.row - op.top] = true;
            }
            
            Pos current = { .row = op.top, .col = op.left };
            for(auto & [pos, data] : op.data){
                while(current.row < pos.row) {
                    ctx.cache_str.append("\n");
                    current.row++;
                    current.col = op.left;
                }

                while(current.col < pos.col){
                    size_t w = lengths[current.col - op.left];
                    if (w > 0) ctx.cache_str.append(gen_space(w + 1));
                    current.col++;
                }

                size_t internal_padding = lengths[pos.col - op.left] - data.len;

                // 加入data中的tags
                // 由于console color只是一个log_tag复用结果
                // 这个table没有权利也没有理由提供颜色自动还原,顶多是提供一个auto_register
                /// @todo 看上面,因此需要用户自己处理颜色还原之类的信息
                // 因此用户需要
                for(LogCustomTag & tag : data.context.tags){
                    tag.set(tag.get() + ctx.cache_str.size());
                    ctx.tags.emplace_back(tag);
                }
                ctx.cache_str.append(data.view());
                ctx.cache_str.append(gen_space(internal_padding + 1));

                current.col++;
            }
            return std::move(ctx);
        }
    };

}

#endif