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
    constexpr int64_t err_io_error = -2;
    
}

#endif