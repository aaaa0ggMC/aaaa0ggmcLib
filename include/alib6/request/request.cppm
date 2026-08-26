/**
 * @file request.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 请求处理与批量窃取队列子系统聚合导出模块 (alib6.request)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.request;

export import :manager;

export namespace alib6 {
    namespace req {
        using alib6::RequestManager;
        using alib6::IsRequestType;
        using alib6::conf_steal_batch_size;
    }
}
