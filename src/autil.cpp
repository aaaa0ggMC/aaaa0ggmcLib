#include <alib5/autil.h>
#include <fstream>

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
    std::ifstream proc("/proc/cpuinfo");
    
    if(proc.good()){
        static std::pmr::string line(ALIB5_DEFAULT_MEMORY_RESOURCE);
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
