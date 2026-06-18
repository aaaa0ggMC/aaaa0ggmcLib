/**
 * @file aerr_id.h
 * @brief Error code identifiers for alib5. / alib5 的错误码定义。
 * @author aaaa0ggmc
 * @date 2026/06/18
 * @version 5.0
 * @copyright Copyright(c) 2026
 */

#ifndef ALIB5_AERR_ID
#define ALIB5_AERR_ID
#include <cstdint>

namespace alib5{
    /// @brief Operation completed successfully (no error). 执行成功
    constexpr int64_t err_success = 0;
    /// @brief std::format failed to format the arguments. std::format错误
    constexpr int64_t err_format_error = -1;
    /// @brief Filesystem access or IO operation failed. filesystem访问io错误
    constexpr int64_t err_filesystem_error = -2;
    /// @brief Generic IO failure not covered by a more specific code. 通用io错误
    constexpr int64_t err_io_error = -3;
    /// @brief Router registration conflict (e.g. duplicate route). 路由冲突
    constexpr int64_t err_conflict_router = -4;
    /// @brief Dump operation failed or conflicted. dump冲突
    constexpr int64_t err_dump_error = -5;
    /// @brief Failed to locate the requested node. 寻找节点冲突
    constexpr int64_t err_locate_error = -6;
    /// @brief No candidate satisfied the matching criteria. 没有任何符合条件
    constexpr int64_t err_none_match = -7;
    /// @brief Internal network layer error (e.g. asio failure). 内部网络错误(比如asio的错误)
    constexpr int64_t err_internal_web_error = -8;
    /// @brief External web/network error emitted by aweb. 网络错误(主要是aweb发出的)
    constexpr int64_t err_web_error = -9;
    /// @brief Input character outside the allowed charset (e.g. trie tree restriction violated). 不满足字符集需求,比如trie tree中限制了字符集但是仍然传入了不允许的字符
    constexpr int64_t err_not_in_charset = -10;
    /// @brief Operation received empty data where data was required. 空白数据
    constexpr int64_t err_empty_data = -11;
    /// @brief Expression evaluation failure. eval错误
    constexpr int64_t err_eval_error = -12;

    /// @brief Maps a web sub-error code v into the reserved web error range (returns -10000 - v). 这里是专门为web开出的错误列表
    constexpr int64_t err_web(int64_t v){return -10000 - v;}
    /// @brief get_client: no client matched the lookup key. get client找不到客户端
    constexpr int64_t err_gclient_not_found = err_web(0);
    /// @brief client::connect invoked on a client that is already connected. client::connect 已经connect了
    constexpr int64_t err_client_connected = err_web(1);
    /// @brief Client operation attempted on a disconnected client. 已经disconnect
    constexpr int64_t err_client_disconnected = err_web(2);
    /// @brief Client queue empty; nothing to dequeue. 空队列
    constexpr int64_t err_client_empty_queue = err_web(3);
}

#endif
