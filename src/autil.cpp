#include <alib5/autil.h>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <deque>
#include <system_error>

using namespace alib5;

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
void sys::enable_virtual_terminal(){
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(hConsole, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsole, consoleMode);
    ///do nothing in linux/unix or others
}
#endif

std::string_view sys::get_cpu_brand(){
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
            for(auto & sub : std::filesystem::directory_iterator(p)){
                FileEntry entry;
                entry.type = (FileEntry::Type)sub.status().type();
                entry.path = sub.path().string();
                
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
                    invoke_error(err_filesystem_error,err.message());
                }else if(cfg.allow[(int)entry.type + 1]){
                    for(auto & fn : cfg.ignore){
                        if(fn(ret.try_get_relative(entry))){
                            keep = false;
                            break;
                        }
                    }
                }else keep = false;
                if(keep)ret.targets_absolute.emplace_back(entry);
            }
        }

        ++current_depth;
        current_layer = next_layer;
        next_layer = {}; 
    }

    return ret;
}
