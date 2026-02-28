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
    /// 内部网络错误(比如asio的错误)
    constexpr int64_t err_internal_web_error = -8;
    /// 网络错误(主要是aweb发出的)
    constexpr int64_t err_web_error = -9;

    /// 这里是专门为web开出的错误列表
    constexpr int64_t err_web(int64_t v){return -10000 - v;}
    /// get client找不到客户端
    constexpr int64_t err_gclient_not_found = err_web(0);
    /// client::connect 已经connect了
    constexpr int64_t err_client_connected = err_web(1);
    // 已经disconnect
    constexpr int64_t err_client_disconnected = err_web(2);
    // 空队列
    constexpr int64_t err_client_empty_queue = err_web(3);
}

#endif