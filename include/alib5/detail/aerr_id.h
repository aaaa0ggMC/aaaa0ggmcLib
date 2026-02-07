#ifndef ALIB5_AERR_ID
#define ALIB5_AERR_ID
#include <cstdint>

namespace alib5{
    /// 执行成功
    constexpr int64_t err_success = 0;
    /// std::format错误
    constexpr int64_t err_format_error = -1;
    /// filesystem访问io错误
    constexpr int64_t err_filesystem_error = -2;
    /// 通用io错误
    constexpr int64_t err_io_error = -3;
    /// 路由冲突
    constexpr int64_t err_conflict_router = -4;
    /// dump冲突
    constexpr int64_t err_dump_error = -5;
    /// 寻找节点冲突
    constexpr int64_t err_locate_error = -6;
    /// 没有任何符合条件
    constexpr int64_t err_none_match = -7;

}

#endif