#include <alib5/aparser.h>

using namespace alib5;

Parser::Parser(std::pmr::memory_resource * __a):
resource(__a),
head(resource),
arg_full(resource),
args(resource){}

std::pmr::vector<panalyser_t> pparset_t::analyse_pipe(){
    std::pmr::vector<panalyser_t> results(resource);
    auto& all_args = this->args;
    auto start_it = all_args.begin();

    for(auto it = all_args.begin(); it != all_args.end(); ++it){
        if(*it == "|"){
            std::pmr::vector<std::string_view> vs;
            for(auto i = start_it;i < it;++i)vs.push_back(*i);
            results.emplace_back(Analyser(*this, vs));
            start_it = it + 1;
        }
    }
    std::pmr::vector<std::string_view> vs;
    for(auto i = start_it;i < all_args.end();++i)vs.push_back(*i);
    results.emplace_back(Analyser(*this, vs));
    return results;
}

/// 这里是为了方便直接生成sub analyser而不一定需要commit
pparset_t::Analyser pcursor_t::sub() noexcept{
    panic_debug(!matched, "Cursor context is invalid!");
    if(applied)return Analyser(
        m_analyser.parser,
        cached_data
    );
    return Analyser{
      m_analyser.parser,
      std::span(data.begin(),
        data.begin() + cursor)  
    };
}

pparset_t::Analyser pcursor_t::invalid() noexcept{
    return Analyser{
      m_analyser.parser,
      {}  
    };
}


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
    if(!processed && prefix != ""){
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
    if(!processed && prefix != ""){
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

std::pair<pvalue_t, pvalue_t> pcursor_t::peek_value_bundle(std::string_view opt) noexcept {
    auto backup_cursor = this->cursor;
    auto backup_processed = this->processed;
    
    auto result = next_value_bundle(opt);
    
    this->cursor = backup_cursor;
    this->processed = backup_processed;
    return result;
}

std::pair<pvalue_t,pvalue_t> pcursor_t::next_value_bundle(std::string_view opt) noexcept{
    panic_debug(!matched, "Cursor context is invalid!");
    if(prefix != "" && !processed){
        return {prefix,next()};
    }
    
    auto s = data[cursor - 1];
    auto pos = s.find(opt);
    Value k (false);
    if(pos != s.npos){
        // 说明找到了
        k = s.substr(0,pos);
    }else{
        // 找不到,那么整个就是key
        k = s;
    }
    // 备份状态
    auto px = this->prefix;
    auto py = this->opt_str;
    auto pp = this->processed;
    processed = false;
    prefix = k;
    opt_str = opt;
    Value v = next();

    prefix = px;
    opt_str = py;
    processed = pp;

    return {k,v};
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