#include <alib5/autil.h>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <deque>
#include <string>
#include <string_view>
#include <system_error>
#ifdef _WIN32
    #include <psapi.h>
#else
    #include <unistd.h>
#endif

using namespace alib5;

#ifdef _WIN32
namespace {
    struct AutoFix {
        AutoFix() noexcept {
            alib5::sys::enable_virtual_terminal();
        }

        ~AutoFix() {
            // 退出时静默重置颜色，防止污染用户的终端环境
            std::fputs("\033[0m", stdout);
        }
    };

    static AutoFix __autofix_trigger;
}
#endif

RuntimeErrorTriggerFn runtime_error_trigger = NULL;
std::vector<RTErrorTriggerFnpp> runtime_error_triggers;

void alib5::invoke_error(detail::ErrorCtx ctx,std::string_view data,Severity sv) noexcept{
    RuntimeError rt{
        .id = ctx.id,
        .data = data,
        .severity = sv,
        .location = ctx.loc
    };

    
    if(runtime_error_trigger)runtime_error_trigger(rt);
    for(auto & fn : runtime_error_triggers){
        [[likely]] if(fn)fn(rt);
    }
}

void alib5::set_fast_error_callback(RuntimeErrorTriggerFn fn) noexcept{
    runtime_error_trigger = fn;
}

void alib5::add_error_callback(RTErrorTriggerFnpp heavy_fn) noexcept{
    runtime_error_triggers.emplace_back(heavy_fn);
}

#ifdef _WIN32
void sys::enable_virtual_terminal() noexcept{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(hConsole, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsole, consoleMode);
    ///do nothing in linux/unix or others
}
#endif

std::string_view sys::get_cpu_brand() noexcept{
    #ifdef _WIN32
    static CHAR tchData[1024] = {0};
    long lRet;
    HKEY hKey;
    DWORD dwSize;
    lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Hardware\\Description\\System\\CentralProcessor\\0",0,KEY_QUERY_VALUE,&hKey);

    if(lRet == ERROR_SUCCESS) {
        dwSize = 1024;
        lRet = RegQueryValueExA(hKey,"ProcessorNameString",0,0,(LPBYTE)tchData,&dwSize);

        tchData[dwSize] = '\0';
        RegCloseKey(hKey);
        if(lRet == ERROR_SUCCESS) {
            return tchData;
        } else {
            return "Unknown";
        }
    }
    return "Unknown";
    #else
    static std::pmr::string line(ALIB5_DEFAULT_MEMORY_RESOURCE);
    std::ifstream proc("/proc/cpuinfo");
    
    if(proc.good()){
        while(std::getline(proc,line)){
            if(line.find("model name") != std::string::npos){
                line = line.substr(line.find(":") + 2);
                break;
            }
        }
        proc.close();
        return line;
    }else return "Unknown";
    #endif
}

void io::TraverseConfig::build() noexcept{
    for(auto val : keep){
        allow[(int)val + 1] = true;
    }
}

io::FileEntry io::load_entry(std::string_view path,bool force_existence) noexcept {
    io::FileEntry entry;
    std::error_code ec;
    auto s = std::filesystem::status(path, ec);


    entry.path = path;
    if(ec){
        if(force_existence){
            auto value = ec.value();
            auto msg = ec.message();
            invoke_error(err_filesystem_error,"{}|{}",value,msg);
        }

        entry.type = io::FileEntry::not_found;
        if(force_existence){
            return entry;
        }
    }else{
        // 有效
        char ch = entry.path.empty() ? '\0' : entry.path.back();
        if(std::filesystem::is_directory(entry.path) && ch != '\\' && ch != '/'){
            entry.path.push_back(ALIB_PATH_SEP);
        }
        entry.last_write = std::filesystem::last_write_time(path,ec);
        entry.type = static_cast<io::FileEntry::Type>(s.type());
    }    
    return entry;
}

void io::FileEntry::scan_subs() const{
    if(scanned)return;
    scanned = true;
    subs.clear();

    TraverseConfig cfg;
    cfg.depth = 1;
    cfg.keep = {
        regular,directory, 
        symlink,block,
        character,fifo,
        socket,unknown
    };
    auto data = io::traverse_files(path,cfg);
    for(auto& entry : data.targets_absolute){
        std::pmr::string key(data.try_get_relative(entry),ALIB5_DEFAULT_MEMORY_RESOURCE);
        subs.emplace(
            std::move(key),
            std::move(entry)
        );
    }
}

const io::FileEntry& io::FileEntry::gen_invalid(){
    static const FileEntry entry;
    return entry;
}

const io::FileEntry& io::FileEntry::operator()(std::string_view str) const{
    if(scanned)return (*this)[str];
    std::filesystem::path rpath(path);
    rpath /= str;
    FileEntry entry = load_entry(rpath.string());
    if(entry.invalid())return gen_invalid();
    auto v = flat_subs.emplace(
        entry.path,
        std::move(entry)
    );
    return v.first->second;
}

const io::FileEntry& io::FileEntry::operator[](std::string_view data) const{
    if(data.empty() || type != directory)return gen_invalid();
    if(!scanned)scan_subs();
    std::pmr::string key(data,ALIB5_DEFAULT_MEMORY_RESOURCE);
    // 对于文件夹可能需要额外处理
    auto it = subs.find(key);
    char ch = key.empty() ? '\0' : key.back();
    if(it == subs.end()){
        if(ch != '/' && ch != '\\'){
            key.push_back('/');
            it = subs.find(key);
            if(it == subs.end()){
                key.back() = '\\';
                it = subs.find(key);
                return (it == subs.end())?(gen_invalid()):(it->second);
            }else return it->second;
        }
        return gen_invalid();
    }
    return it->second;
}

io::TraverseData io::traverse_files(std::string_view file_path,TraverseConfig cfg) noexcept{
    io::TraverseData ret;
    auto root = std::filesystem::path(file_path);
    if(root.is_relative()){
        root = std::filesystem::current_path() / root;
        ret.root_absolute = std::filesystem::current_path().string();
    }else ret.root_absolute = root.string();
    if(ret.root_absolute.empty()){
        invoke_error(err_filesystem_error,"cannot locate root directory!");
        return {};
    }
    if(ret.root_absolute.back() != '/' && ret.root_absolute.back() != '\\'){
        ret.root_absolute.push_back(ALIB_PATH_SEP);
    }

    // 构建小东西
    cfg.build();

    std::error_code err;
    if(!std::filesystem::is_directory(root,err)){
        invoke_error(err_filesystem_error,"\"{}\" is not a directory!",root.string());
    }else if(err){
        invoke_error(err_filesystem_error,err.message());
        return {};
    }

    int current_depth = 0;
    std::deque<std::filesystem::path> current_layer;
    std::deque<std::filesystem::path> next_layer;
    current_layer.emplace_back(root);

    while(!current_layer.empty() && (current_depth < cfg.depth || cfg.depth < 0)){
        for(auto & p : current_layer){
            std::filesystem::directory_iterator direc;

            try{
                direc = std::filesystem::directory_iterator(p);
            }catch(std::filesystem::filesystem_error e){
                // do nothing,遇到了permission deny等错误，这个需要报错吗？尝试报错一下吧
                auto code = e.code().value();
                auto what_msg = e.what();
                invoke_error(err_filesystem_error,"{}|{}",code,what_msg);
            }catch(...){
                invoke_error(err_filesystem_error,"{}","Unknown");
            }

            for(auto & sub : direc){
                try{
                    FileEntry entry;
                    entry.type = (FileEntry::Type)sub.status().type();
                    entry.path = sub.path().string();
                    entry.last_write = sub.last_write_time();
                    
                    if(entry.path.empty()){
                        continue;
                    }

                    bool keep = true;
                    
                    if(std::filesystem::is_directory(sub,err)){
                        if(entry.path.back() != '/' && entry.path.back() != '\\'){
                            entry.path.push_back(ALIB_PATH_SEP);
                        }
                        if(current_depth + 1 != cfg.depth)next_layer.push_back(entry.path);
                    }
                    if(err){
                        auto msg = err.message();
                        invoke_error(err_filesystem_error,"{} with file {}",msg,entry.path);
                    }
                    if(cfg.allow[(int)entry.type + 1]){
                        for(auto & fn : cfg.ignore){
                            if(fn(ret.try_get_relative(entry))){
                                keep = false;
                                break;
                            }
                        }
                    }else keep = false;
                    if(keep)ret.targets_absolute.emplace_back(entry);
                }catch(std::filesystem::filesystem_error e){
                    // do nothing,遇到了permission deny等错误，这个需要报错吗？尝试报错一下吧
                    auto code = e.code().value();
                    auto what_msg = e.what();
                    invoke_error(err_filesystem_error,"{}|{}",code,what_msg);
                }catch(...){
                    invoke_error(err_filesystem_error,"{}","Unknown");
                }
            }
        }

        ++current_depth;
        current_layer = next_layer;
        next_layer = {}; 
    }

    return ret;
}

/// 返回格式化的当前时间
std::string_view misc::get_time() noexcept {
    static thread_local std::pmr::string buffer (ALIB5_DEFAULT_MEMORY_RESOURCE);
    static thread_local bool inited = [&]{
        buffer.reserve(conf_generic_string_reserve_size);
        return true;
    }();

    time_t rawtime;
    struct tm ptminfo;
    time(&rawtime);
    localtime_r(&rawtime,&ptminfo);
    buffer.clear();
    std::format_to(std::back_inserter(buffer), 
        "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
        ptminfo.tm_year + 1900, 
        ptminfo.tm_mon + 1, 
        ptminfo.tm_mday,
        ptminfo.tm_hour, 
        ptminfo.tm_min, 
        ptminfo.tm_sec
    );
    return buffer;
}


sys::ProgramMemUsage sys::get_prog_mem_usage() noexcept {
    ProgramMemUsage usage {0, 0};
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    // GetCurrentProcess() 返回的是伪句柄，无需打开也无需关闭，开销极低
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        usage.memory = static_cast<mem_bytes>(pmc.WorkingSetSize);
        usage.virt_memory = static_cast<mem_bytes>(pmc.PagefileUsage);
    }
#else
    // Linux 下只读 /proc/self/status 即可，没必要碰 /proc/self/stat
    // status 里的 VmRSS 和 VmSize 已经非常准备且易于解析
    if (FILE* f = std::fopen("/proc/self/status", "r")) {
        char line[128];
        int found = 0;
        while (std::fgets(line, sizeof(line), f) && found < 2) {
            // 使用 sscanf 直接提取数值，避开繁琐的 stringstream
            if (std::sscanf(line, "VmSize: %ld", &usage.virt_memory) == 1) {
                usage.virt_memory *= 1024; // 转换 KB 到字节
                found++;
            } else if (std::sscanf(line, "VmRSS: %ld", &usage.memory) == 1) {
                usage.memory *= 1024; // 转换 KB 到字节
                found++;
            }
        }
        std::fclose(f);
    }
#endif
    return usage;
}

sys::GlobalMemUsage sys::get_global_mem_usage() noexcept {
    GlobalMemUsage ret {};

#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        ret.physical_total = statex.ullTotalPhys;
        ret.physical_used  = statex.ullTotalPhys - statex.ullAvailPhys;
        ret.virtual_total  = statex.ullTotalVirtual;
        ret.virtual_used   = statex.ullTotalVirtual - statex.ullAvailVirtual;
        ret.page_total     = statex.ullTotalPageFile;
        ret.page_used      = statex.ullTotalPageFile - statex.ullAvailPageFile;
        ret.percent        = statex.dwMemoryLoad;
    }
#else
    // Linux 实现：单次读取 /proc/meminfo
    if (FILE* f = std::fopen("/proc/meminfo", "r")) {
        char line[128];
        long long total = 0, free = 0, buffers = 0, cached = 0, reclaimable = 0, s_total = 0, s_free = 0;
        
        while (std::fgets(line, sizeof(line), f)) {
            // 使用 sscanf 直接解析，跳过 std::string 构造
            if (std::sscanf(line, "MemTotal: %lld", &total) == 1) continue;
            if (std::sscanf(line, "MemFree: %lld", &free) == 1) continue;
            if (std::sscanf(line, "Buffers: %lld", &buffers) == 1) continue;
            if (std::sscanf(line, "Cached: %lld", &cached) == 1) continue;
            if (std::sscanf(line, "SReclaimable: %lld", &reclaimable) == 1) continue;
            if (std::sscanf(line, "SwapTotal: %lld", &s_total) == 1) continue;
            if (std::sscanf(line, "SwapFree: %lld", &s_free) == 1) continue;
        }
        std::fclose(f);

        // Linux 内存计算细节：Used = Total - (Free + Buffers + Cached + SReclaimable)
        // SReclaimable 是内核可回收的 Slab，通常也算作可用内存
        long long available = free + buffers + cached + reclaimable;
        
        ret.physical_total = total * 1024;
        ret.physical_used  = (total - available) * 1024;
        ret.page_total     = s_total * 1024;
        ret.page_used      = (s_total - s_free) * 1024;
        
        // 虚拟内存占位
        ret.virtual_total  = 0;
        ret.virtual_used   = 0;

        if (ret.physical_total > 0) {
            ret.percent = static_cast<unsigned int>((ret.physical_used * 100) / ret.physical_total);
        }
    }
#endif
    return ret;
}

std::string_view str::unescape(std::string_view in) noexcept {
    static thread_local std::pmr::string buffer {ALIB5_DEFAULT_MEMORY_RESOURCE};
    buffer.clear();

    for (size_t i = 0; i < in.size(); ++i){
        if(in[i] == '\\' && i + 1 < in.size()){
            i++; 
            switch(in[i]){
                case 'a':  buffer += '\a'; break;
                case 'b':  buffer += '\b'; break;
                case 'f':  buffer += '\f'; break;
                case 'n':  buffer += '\n'; break;
                case 'r':  buffer += '\r'; break;
                case 't':  buffer += '\t'; break;
                case 'v':  buffer += '\v'; break;
                case 'e':  buffer += '\x1b'; break;
                case '\\': buffer += '\\'; break;
                case '\'': buffer += '\''; break;
                case '\"': buffer += '\"'; break;
                case 'x': {
                    if(i + 1 < in.size() && isxdigit(in[i+1])){
                        int val = 0;
                        for(int k = 0; k < 2 && i + 1 < in.size() && isxdigit(in[i+1]); ++k){
                            char c = in[++i];
                            val = val * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
                        }
                        buffer += static_cast<char>(val);
                    }
                    break;
                }
                case 'u': 
                case 'U': {
                    int len = (in[i] == 'U') ? 8 : 4;
                    if(i + len < in.size()){
                        uint32_t val = 0;
                        bool valid = true;
                        for(int k = 1; k <= len; ++k){
                            if(isxdigit(in[i+k])){
                                char c = in[i+k];
                                val = val * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
                            }else{
                                valid = false;
                                break;
                            }
                        }
                        if(valid){
                            i += len;
                            if(val <= 0x7F){
                                buffer += static_cast<char>(val);
                            }else if(val <= 0x7FF){
                                buffer += static_cast<char>(0xC0 | ((val >> 6) & 0x1F));
                                buffer += static_cast<char>(0x80 | (val & 0x3F));
                            }else if(val <= 0xFFFF){
                                buffer += static_cast<char>(0xE0 | ((val >> 12) & 0x0F));
                                buffer += static_cast<char>(0x80 | ((val >> 6) & 0x3F));
                                buffer += static_cast<char>(0x80 | (val & 0x3F));
                            }else if(val <= 0x10FFFF){
                                buffer += static_cast<char>(0xF0 | ((val >> 18) & 0x07));
                                buffer += static_cast<char>(0x80 | ((val >> 12) & 0x3F));
                                buffer += static_cast<char>(0x80 | ((val >> 6) & 0x3F));
                                buffer += static_cast<char>(0x80 | (val & 0x3F));
                            }
                            break;
                        }
                    }
                    buffer += in[i];
                    break;
                }
                default: {
                    if(in[i] >= '0' && in[i] <= '7'){
                        int val = 0;
                        for (int k = 0; k < 3 && i < in.size() && (in[i] >= '0' && in[i] <= '7'); ++k){
                            val = val * 8 + (in[i++] - '0');
                        }
                        i--; 
                        buffer += static_cast<char>(val);
                    }else{
                        buffer += in[i];
                    }
                }
            }
        }else{
            buffer += in[i];
        }
    }
    return buffer;
}

std::string_view str::escape(std::string_view in,bool ensure_ascii) noexcept {
    static thread_local std::pmr::string buffer {ALIB5_DEFAULT_MEMORY_RESOURCE};
    buffer.clear();

    static const char* hex = "0123456789abcdef";

    for (size_t i = 0; i < in.size(); ++i){
        unsigned char c = in[i];
        switch(c){
            case '\"': buffer += "\\\""; break;
            case '\\': buffer += "\\\\"; break;
            case '\a': buffer += "\\a";  break;
            case '\b': buffer += "\\b";  break;
            case '\f': buffer += "\\f";  break;
            case '\n': buffer += "\\n";  break;
            case '\r': buffer += "\\r";  break;
            case '\t': buffer += "\\t";  break;
            case '\v': buffer += "\\v";  break;
            case '\x1b': buffer += "\\e"; break;
            default: {
                if(c < 32 || c == 127){ // 控制字符使用 \xHH
                    buffer += "\\x";
                    buffer += hex[(c >> 4) & 0xF];
                    buffer += hex[c & 0xF];
                }else if(ensure_ascii && c >= 0x80){ // 处理 UTF-8 并转为 \u 或 \U
                    uint32_t cp = 0;
                    size_t len = 0;
                    // 简单的 UTF-8 解码逻辑
                    if((c & 0xE0) == 0xC0 && i + 1 < in.size()){
                        cp = (c & 0x1F) << 6 | (in[++i] & 0x3F);
                        len = 4; // \uXXXX
                    }else if((c & 0xF0) == 0xE0 && i + 2 < in.size()){
                        cp = (c & 0x0F) << 12 | ((in[i+1] & 0x3F) << 6) | (in[i+2] & 0x3F);
                        i += 2; len = 4;
                    }else if((c & 0xF8) == 0xF0 && i + 3 < in.size()){
                        cp = (c & 0x07) << 18 | ((in[i+1] & 0x3F) << 12) | ((in[i+2] & 0x3F) << 6) | (in[i+3] & 0x3F);
                        i += 3; len = 8; // \UXXXXXXXX
                    }

                    if(len == 4){
                        buffer += "\\u";
                        for(int k = 12; k >= 0; k -= 4) buffer += hex[(cp >> k) & 0xF];
                    }else if(len == 8){
                        buffer += "\\U";
                        for(int k = 28; k >= 0; k -= 4) buffer += hex[(cp >> k) & 0xF];
                    }else{
                        // 非法 UTF-8 序列，退回到 \x 处理
                        buffer += "\\x";
                        buffer += hex[(c >> 4) & 0xF];
                        buffer += hex[c & 0xF];
                    }
                }else{
                    buffer += c;
                }
                break;
            }
        }
    }
    return buffer;
}

std::string_view str::trim(std::string_view input){
    // 找到首位就行
    const char * blanks = "\f\v\r\t\n ";

    auto head = input.find_first_not_of(blanks);
    auto tail = input.find_last_not_of(blanks);

    if(head == input.npos)return "";
    if(tail == input.npos)tail = input.size();

    return input.substr(head,tail - head + 1);
}

std::vector<std::string_view> str::split(std::string_view source,std::string_view sep) noexcept{
    std::vector<std::string_view> vec;
    std::string_view finder = source;
    size_t step = sep.size();

    while(true){
        auto result = finder.find(sep);
        if(result == finder.npos){
            vec.push_back(finder);
            break;
        }
        vec.push_back(finder.substr(0,result));
        finder = finder.substr(result + step);
    }

    return vec;
}


std::string_view misc::format_duration(int secs) noexcept{
    static thread_local std::pmr::string buffer (ALIB5_DEFAULT_MEMORY_RESOURCE);
    static thread_local bool inited = [&]{
        buffer.reserve(conf_generic_string_reserve_size);
        return true;
    }();
    
    int sec = secs % 60;
    secs /= 60;
    int min = secs % 60;
    secs /= 60;
    int hour= secs % 60;
    secs /= 60;
    int day = secs % 24;
    secs /= 24;
    int year = secs % 356;


    buffer.clear();
    #define GEN_V(year,s) if(year != 0){\
        buffer += std::to_string(year);\
        buffer += s " ";\
    }
    
    GEN_V(year, "y");
    GEN_V(day, "d");
    GEN_V(hour, "h");
    GEN_V(min, "m");
    GEN_V(sec, "s");

    #undef GEN_V
    return buffer;
}