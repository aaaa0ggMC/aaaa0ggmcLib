/**@file abase.h
* @brief 杂七杂八的宏等等
* @author aaaa0ggmc
* @date 2026/01/21
* @version 5.0
* @copyright Copyright(c) 2026
*/
#ifndef ALIB5_ABASE
#define ALIB5_ABASE
/// 平台相关的定义
#ifdef _WIN32 
    #include <windows.h>
    #ifndef ALIB5_API
        #ifdef BUILD_DLL
            #define ALIB5_API __declspec(dllexport)
        #else
            #define ALIB5_API __declspec(dllimport)
        #endif
    #endif
    using mem_bytes = __int64;

#elif __linux__
    #include <unistd.h>
    #ifndef DLL_EXPORT
        #define ALIB5_API
    #endif
    using mem_bytes = __int64_t;

#endif

/// 颜色相关定义
#ifndef __ALIB_CONSOLE_COLORS
#define __ALIB_CONSOLE_COLORS
//=============== 基础样式 ================
#define ACP_RESET       "\e[0m"   // 重置所有样式
#define ACP_BOLD        "\e[1m"   // 加粗
#define ACP_DIM         "\e[2m"   // 暗化
#define ACP_ITALIC      "\e[3m"   // 斜体
#define ACP_UNDERLINE   "\e[4m"   // 下划线
#define ACP_BLINK       "\e[5m"   // 慢闪烁
#define ACP_BLINK_FAST  "\e[6m"   // 快闪烁
#define ACP_REVERSE     "\e[7m"   // 反色（前景/背景交换）
#define ACP_HIDDEN      "\e[8m"   // 隐藏文字
//=============== 标准前景色 ================
#define ACP_BLACK     "\e[30m"
#define ACP_RED       "\e[31m"
#define ACP_GREEN     "\e[32m"
#define ACP_YELLOW    "\e[33m"
#define ACP_BLUE      "\e[34m"
#define ACP_MAGENTA   "\e[35m"
#define ACP_CYAN      "\e[36m"
#define ACP_GRAY      "\e[36m"
#define ACP_WHITE     "\e[37m"
//=============== 标准背景色 ================
#define ACP_BG_BLACK   "\e[40m"
#define ACP_BG_RED     "\e[41m"
#define ACP_BG_GREEN   "\e[42m"
#define ACP_BG_YELLOW  "\e[43m"
#define ACP_BG_BLUE    "\e[44m"
#define ACP_BG_MAGENTA "\e[45m"
#define ACP_BG_CYAN    "\e[46m"
#define ACP_BG_GRAY    "\e[46m"
#define ACP_BG_WHITE   "\e[47m"
//=============== 亮色模式 ================
// 前景亮色（如高亮红/绿等）
#define ACP_LRED     "\e[91m"
#define ACP_LGREEN   "\e[92m"
#define ACP_LYELLOW  "\e[93m"
#define ACP_LBLUE    "\e[94m"
#define ACP_LMAGENTA "\e[95m"
#define ACP_LCYAN    "\e[96m"
#define ACP_LGRAY    "\e[96m"
#define ACP_LWHITE   "\e[97m"
// 背景亮色
#define ACP_BG_LRED     "\e[101m"
#define ACP_BG_LGREEN   "\e[102m"
#define ACP_BG_LYELLOW  "\e[103m"
#define ACP_BG_LBLUE    "\e[104m"
#define ACP_BG_LMAGENTA "\e[105m"
#define ACP_BG_LCYAN    "\e[106m"
#define ACP_BG_LGRAY    "\e[106m"
#define ACP_BG_LWHITE   "\e[107m"

//256色模式
#define ACP_FG256(n) "\e[38;5;" #n "m"
#define ACP_BG256(n) "\e[48;5;" #n "m"

//RGB真彩色模式
#define ACP_FGRGB(r,g,b) "\e[38;2;" #r ";" #g ";" #b "m"
#define ACP_BGRGB(r,g,b) "\e[48;2;" #r ";" #g ";" #b "m"
#endif

/// 关于路径
#ifdef _WIN32
    #define ALIB_PATH_SEP '\\'
    #define ALIB_PATH_SEPS "\\"
#else
    #define ALIB_PATH_SEP '/'
    #define ALIB_PATH_SEPS "/"
#endif

#endif