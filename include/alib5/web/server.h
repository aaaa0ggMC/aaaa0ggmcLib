#ifndef ALIB5_AWEB_SERVER
#define ALIB5_AWEB_SERVER
#include <alib5/autil.h>
#include <alib5/aref.h>
#include <alib5/web/base.h>
#include <alib5/ecs/component_pool.h>
#include <deque>
#include <cstring>
#include <semaphore>

namespace alib5::web{
    constexpr uint32_t conf_clients_reserve = 16;

    /// 客户端缓存
    struct ALIB5_API ClientPending{
        /// 当前客户端缓存的version
        uint32_t version {0};

        /// 客户端的数据锁,暂时没场景
        // std::unique_ptr<std::mutex> data_lock;

        using container_t = ecs::detail::LinearStorage<DataPack>;
        /// 包头大小
        std::array<std::byte,conf_header_size> header_buffer;
        /// 这个是包头写入
        PackageHeader header_scratch;
        
        container_t package_storage;        
        /// 存储的是一个offset,linearstorage选择的是freelist因此还算安全
        std::pmr::deque<size_t> packages;

        /// 客户端的锁
        std::unique_ptr<std::mutex> queue_lock;
        /// 这里保存client是否在进行写入操作
        bool is_writing { false };
        /// 写队列
        std::pmr::deque<DataPack> write_queue;

        ClientPending(std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
        //:data_lock(std::make_unique<std::mutex>())
        :queue_lock(std::make_unique<std::mutex>())
        ,packages(__a)
        ,write_queue(__a)
        ,package_storage(0,__a,__a){}

        // 由于d_cleanup该做的都做了,这里的作用是阻止析构函数
        void reset(){}

        void d_cleanup(){
            ++version;
            // printf("dcleanup\n");
            packages.clear();
            package_storage.clear();
            write_queue.clear();
            is_writing = false;
        }
    };

    struct ALIB5_API ServerConfig{
        /// 地址
        Address address;
        /// 对于相同binding的server pool,这个是必要的
        bool reuse_port { true };
        /// 是否输出asio发现的错误
        bool report_internal_errors { true };
        /// 重大错误重试时间,比如fd耗尽
        std::chrono::microseconds retry_time { std::chrono::milliseconds(100) };
        /// 单个包内容最大长度,默认为2MB
        size_t single_pack_max_size { 2 * 1024 * 1024 };
        /// 单个server并行线程数目
        int64_t concurrent_workers { -1 };
        /// 发包策略
        enum Strategy{
            Time, // 严格时间次序
            Package // 包先满就发送
        };
        Strategy strategy { Time };

        ServerConfig(){}
        ServerConfig(Address add):address(add){}
    };

    /// 核心服务器,相当于是对asio的进一步封装
    /// 支持TCP/UDP协议层分发
    struct ALIB5_API Server{
        enum Status{
            Stopped, ///< 未启动状态
            Running  ///< 运行状态
        };
        
        ProtocolType protocol_type;
        Status server_status { Stopped };
        ServerConfig config;
        uint64_t max_version;
        std::counting_semaphore<> semaphore;

        ALIB5_API Server(
            const ServerConfig& cfg = ServerConfig(),
            ProtocolType type = ProtocolType::TCP,
            std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE,
            size_t reserve_count = conf_clients_reserve
        );
        ALIB5_API ~Server();

        inline uint64_t gen_version(){
            return ++max_version;
        }

        inline bool start(){
            // 已经启动过了
            if(server_status != Stopped)return true;
            if(__internal_start()){
                server_status = Running;
                return true;
            }else return false;
        }

        bool ALIB5_API try_pop(DataPack & pack,size_t& client_id);

        inline auto get_allocator(){
            return res;
        }

        /// 生命周期必须小于等于Server!
        struct Client{
            // 小于0为无效client
            int64_t client_id { -1 };
            Server * server { nullptr };

            ALIB5_API Client();
            ALIB5_API ~Client();

            struct Impl;
            std::unique_ptr<Impl> pimpl;

            bool send(const DataPack & pack);
            bool async_send(const DataPack& pack);
            bool async_send(DataPack && pack);
        };
        int64_t get_client(size_t id,Client & client);

        inline bool wait_pop(DataPack & pack,size_t & client_id){
            while(true){
                if(server_status == Stopped)return false;
                if(try_pop(pack,client_id))break;
                semaphore.acquire();
                if(try_pop(pack,client_id))break;
            }
            return true;
        }

        inline bool wait_pop(DataPack & pack,Client& client){
            size_t index;
            if(wait_pop(pack,index)){
                get_client(index,client);
                return true;
            }
            return false;
        }

        inline bool try_pop(DataPack & pack,Client& client){
            size_t index;
            if(try_pop(pack,index)){
                get_client(index,client);
                return true;
            }
            return false;
        }

        auto send(size_t client_id,const DataPack & pack){return __internal_send(client_id,pack);}
        auto async_send(size_t client_id,const DataPack & pack){return __internal_async_send(client_id,pack);}
        auto async_send(size_t client_id,DataPack && pack){return __internal_async_send(client_id,std::move(pack));}
        
        auto send(Client& client,const DataPack & pack){return __internal_send(client.client_id,pack);}
        auto async_send(Client& client,const DataPack & pack){return __internal_async_send(client.client_id,pack);}
        auto async_send(Client& client,DataPack && pack){return __internal_async_send(client.client_id,std::move(pack));}
        
        bool ALIB5_API __internal_send(size_t client_id,const DataPack & pack);
        /// 这个是copy
        inline bool __internal_async_send(size_t client_id,const DataPack & pack){
            DataPack p = pack;
            return __internal_async_send(client_id,std::move(p));
        }
        /// 这个是move
        bool ALIB5_API __internal_async_send(size_t client_id,DataPack && pack);
    private:
        bool ALIB5_API __internal_start();
        void ALIB5_API __internal_setup();
        void do_write_loop(size_t client_id);

        struct PackQueueData{
            /// client_id为对应在established_clients中的槽位,这样支持用户自己获取详细信息
            size_t client_id;
            DataPack pack;
        };

        struct Impl;
        std::pmr::memory_resource * res;
        std::unique_ptr<Impl> pimpl;
        std::pmr::deque<PackQueueData> pack_queue;
        ecs::detail::LinearStorage<ClientPending,std::pmr::unordered_map<size_t,ClientPending>> established_clients;
        std::mutex data_lock;
        std::mutex mapper_lock;
    };

    inline DataPack make_pack(Server & server,std::string_view s,PackCategory cat = PackCategory::Data){
        return make_pack(*server.get_allocator(),s,server.gen_version(),cat);
    }
}

namespace alib5::web{
    inline bool Server::Client::send(const DataPack & pack){
        [[unlikely]] if(!server)return false;
        return server->send(client_id,pack);
    }

    inline bool Server::Client::async_send(const DataPack & pack){
        [[unlikely]] if(!server)return false;
        return server->async_send(client_id,pack);
    }

    inline bool Server::Client::async_send(DataPack && pack){
        [[unlikely]] if(!server)return false;
        return server->async_send(client_id,std::move(pack));
    }
}

#endif