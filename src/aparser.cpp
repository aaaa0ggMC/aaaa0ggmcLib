#include <alib5/aparser.h>

using namespace alib5;

Parser::Parser(std::pmr::memory_resource * __a):
resource(__a),
head(resource),
arg_full(resource),
args(resource){}

Parser::Analyser Parser::Analyser::Cursor::commit() noexcept{
    panic_debug(!matched, "Cursor context is invalid!");
    if(applied)return Analyser(
        m_analyser.parser,
        cached_data
    );
    applied = true;
    cached_data.clear();
    cached_data.insert(
        cached_data.end(),
        data.begin(),
        data.begin() + cursor);
    
    m_analyser.inputs.erase(
        m_analyser.inputs.begin() + parent_index,
        m_analyser.inputs.begin() + parent_index + cursor
    );
    return Analyser(
        m_analyser.parser,
        cached_data
    );
}

Parser::Analyser::Value Parser::Analyser::Cursor::peek() const noexcept{
    panic_debug(!matched, "Cursor context is invalid!");
    size_t cursor = this->cursor;
    if(!processed && cursor == 1 && prefix != ""){
        auto s = data[cursor - 1];
        if(s.starts_with(prefix)){
            size_t sz = prefix.size();
            // 尝试匹配optional
            if(!opt_str.empty()){
                if(s.substr(prefix.size()).starts_with(opt_str)){
                    sz += opt_str.size();       
                }else if(cursor < data.size() && data[cursor].starts_with(opt_str)){
                    s = data[cursor++];
                    sz = opt_str.size();
                }
            }
            // 完全匹配原本
            s = str::trim(s.substr(sz));
            if(!s.empty())return s;
            // 否则回退到下面的                            
        }
    }
    if(cursor >= data.size())return "";
    return data[cursor++];
}

Parser::Analyser::Value Parser::Analyser::Cursor::next() noexcept{
    panic_debug(!matched, "Cursor context is invalid!");
    if(!processed && cursor == 1 && prefix != ""){
        processed = true;
        auto s = data[cursor - 1];
        if(s.starts_with(prefix)){
            size_t sz = prefix.size();
            // 尝试匹配optional
            if(!opt_str.empty()){
                if(s.substr(prefix.size()).starts_with(opt_str)){
                    sz += opt_str.size();       
                }else if(cursor < data.size() && data[cursor].starts_with(opt_str)){
                    s = data[cursor++];
                    sz = opt_str.size();
                }
            }
            // 完全匹配原本
            s = str::trim(s.substr(sz));
            if(!s.empty())return s;
            // 否则回退到下面的                            
        }
    }
    if(cursor >= data.size())return "";
    return data[cursor++];
}

Parser::Analyser::Cursor Parser::Analyser::with_prefix(std::string_view data,std::string_view opt) noexcept{
    for(size_t i = 1;i < inputs.size();++i){
        if(inputs[i].starts_with(data)){
            auto c = Cursor{
            *this,
            std::span(inputs.begin() + i,inputs.end()),
            i,
            true};
            c.prefix = data;
            c.opt_str = opt;
            return c;
        }
    }
    return Cursor{
        *this,
        inputs,
        0,
        false
    };
}

Parser::Analyser::Cursor Parser::Analyser::with_opt(std::string_view opt) noexcept{
    // 忽略掉header
    for(size_t i = 1;i < inputs.size();++i){
        if(inputs[i] == opt)return Cursor{
            *this,
            std::span(inputs.begin() + i,inputs.end()),
            i,
            true
        };
    }
    return Cursor{
        *this,
        inputs,
        0,
        false
    };
}

void Parser::from_args(int argc,const char * argv[]) noexcept{
    head.clear();
    arg_full.clear();
    args.clear();
    
    for(int i = 0;i < argc;++i){
        std::pmr::string d(argv[i],resource);

        if(i != 0){
            // 这里尝试去模拟原始行
            arg_full.push_back('\"');
            arg_full += str::escape(d);
            arg_full.append("\" ");
        }

        args.emplace_back(std::move(d));
    }

    head = args[0];
}

void Parser::parse(std::string_view data,bool slice_args){
    head.clear();
    arg_full.clear();
    args.clear();

    // 移除空白
    std::string_view process = data;

    auto is_meta = [](char ch) {
        switch(ch){
            case '|': case '&': case ';': 
            case '(': case ')': case '<': case '>':
                return true;
            default:
                return false;
        }
    };

    // 提取head
    auto extract_sector = [&]{
        int pos = 0;

        bool string_mode = false;
        bool escape_mode = false;
        char meta_ch = '\0';
        if(process[0] == '"'){
            string_mode = true;
            process = process.substr(1);
        }
        for(auto ch : process){
            if(!escape_mode && !string_mode){
                if(std::isspace(ch) || ch == '"')break;
                else if(is_meta(ch)){
                    if(pos == 0){
                        ++pos;
                    }
                    break;
                }
            }
            if(ch == '"' && string_mode && !escape_mode)break;
            ++pos;
            if(ch == '\\'){
                escape_mode = !escape_mode;
                continue;
            }
            if(escape_mode)escape_mode = false;
        }
        std::string_view sector = process.substr(0,pos);
        
        if(pos < process.size())process = process.substr(pos + (int)string_mode);
        else process = "";

        if(args.size() == 0){
            arg_full = str::trim(process);
        }
        return sector;
    };

    while(true){
        process = str::trim(process);
        if(process.empty())break;
        std::pmr::string d(str::unescape(extract_sector()),resource);
        args.emplace_back(std::move(d));
        [[unlikely]] if(!slice_args)break;
    }

    // 生成head
    if(!args.empty()){
        head = args[0];
    }
}