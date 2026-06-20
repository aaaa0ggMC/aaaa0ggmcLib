/**
 * @file client.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 客户端，包含TCP&UDP部分
 * @version 5.0
 * @date 2026-06-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ALIB5_ANET_CLIENT_H
#define ALIB5_ANET_CLIENT_H
#include <alib5/autil.h>

namespace alib5::web{
    
    class ALIB5_API Client{
        virtual void send(){}
        virtual void receive(){}
    };

    class ALIB5_API TCPClient : public Client{


    };

    class ALIB5_API UDPClient : public Client{

    };
}


#endif