#ifndef ALIB5_AWEB_CLIENT
#define ALIB5_AWEB_CLIENT
#include <alib5/web/base.h>
#include <semaphore>
#include <deque>

namespace alib5::web{

    /// 客户端配置
    struct ALIB5_API ClientConfig{
        /// 是否发出内部错误
        bool report_internal_errors {true};
        /// 单个client并行数目,默认单行
        size_t concurrent_workers { 1 };
        /// 单包最大大小
        size_t single_pack_max_size { 2 * 1024 * 1024 };

    };

    /// 真正的客户端
    struct ALIB5_API Client{
        enum Status{
            Disconnected,
            Connected
        };
        
        ProtocolType protocol_type;
        Status client_status;
        ClientConfig config;
        uint32_t max_version;


        Client(ClientConfig = ClientConfig(),ProtocolType type = ProtocolType::TCP,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE);
        ~Client();

        int64_t async_connect(const Address & addr,std::function<void(std::error_code)> fin_callback = nullptr);
        int64_t connect(const Address & addr);
        int64_t connect(std::string_view host,uint16_t port){return connect(resolve(host,port));}
        int64_t async_connect(std::string_view host,uint16_t port){return async_connect(resolve(host,port));}
        int64_t disconnect();

        int64_t peek(DataPack & pack);
        int64_t receive(DataPack & pack);    
        auto get_allocator(){return res;}
    
    
        auto send(const DataPack & pack){return __internal_send(pack);}
        auto async_send(DataPack && pack){return __internal_async_send(std::move(pack));}
        auto async_send(const DataPack & pack){DataPack p(pack);return __internal_async_send(std::move(p));};
        auto gen_version(){return ++max_version;}


        bool __internal_send(const DataPack & pack);
        bool __internal_async_send(DataPack && pack);
    private:
        std::pmr::memory_resource * res;
        struct Impl;
        std::unique_ptr<Impl> pimpl;
        std::counting_semaphore<> semaphore;

        std::mutex data_lock;
        std::pmr::deque<DataPack> queue;
    };

    inline DataPack make_pack(Client & client,std::string_view s,PackCategory cat = PackCategory::Data){
        return make_pack(*client.get_allocator(),s,client.gen_version(),cat);
    }
}

#endif