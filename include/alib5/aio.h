/**
 * @file aio.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 提供简单的io/vfs支持
 * @version 5.0
 * @date 2026-03-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_AIO
#define ALIB5_AIO
#include <alib5/autil.h>

namespace alib5::aio{
    struct ALIB5_API Context{
        Context();
        ~Context();
        static Context& global();
        /// 返回旧版本的context,不建议在初始化后运行中切换
        static Context& set_global(Context &);
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    // 其实和autil io::FileEntry::Type是一样的
    enum class FileType : int32_t{
        None = 0,
        NotFound = -1, 
        Regular = 1,
        Directory = 2, 
        Symlink = 3,
        Block = 4,
        Character = 5,
        Fifo = 6,
        Socket = 7, 
        Unknown = 8
    };

    // 我觉得这个结构还算轻量,因此数据返回就是单纯的copy啥的
    struct ALIB5_API File{
        FileType type;
    };

}

#endif