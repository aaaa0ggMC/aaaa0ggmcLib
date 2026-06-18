/**
 * @file aio.h
 * @brief Lightweight IO and virtual filesystem support. / 提供简单的 io/vfs 支持
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */
#ifndef ALIB5_AIO
#define ALIB5_AIO
#include <alib5/autil.h>

namespace alib5::aio{
    /**
     * @brief Owning IO context that owns the global VFS / runtime state.
     *
     * @par Original Comment:
     * 返回旧版本的context,不建议在初始化后运行中切换
     */
    struct ALIB5_API Context{
        Context();
        ~Context();
        static Context& global();
        /// @brief Replaces the active global context; returns the previously-installed one. 返回旧版本的context,不建议在初始化后运行中切换
        static Context& set_global(Context &);
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    /**
     * @brief File type tag mirroring io::FileEntry::Type values from autil.h.
     *
     * @par Original Comment:
     * 其实和autil io::FileEntry::Type是一样的
     */
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

    /**
     * @brief Lightweight file metadata snapshot returned by value.
     *
     * @par Original Comment:
     * 我觉得这个结构还算轻量,因此数据返回就是单纯的copy啥的
     */
    struct ALIB5_API File{
        /// @brief Resolved file type tag.
        FileType type;
    };

}

#endif
