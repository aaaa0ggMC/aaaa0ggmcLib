#include <alib5/aweb.h>
#include <asio.hpp>
#include <thread>

using namespace alib5;
using namespace alib5::web;

/// 用于解析数据
static asio::io_context analyser_context;

Address web::resolve(std::string_view address,uint16_t port){
    asio::ip::tcp::resolver resolver(analyser_context);
    std::error_code ec;
    auto results = resolver.resolve(address, std::to_string(port), ec);

    Address res_addr;
    if(ec){
        invoke_error(err_internal_web_error,ec.message());
        return res_addr;
    }

    if(!results.empty()){
        auto endpoint = results.begin()->endpoint();
        auto asio_addr = endpoint.address();

        res_addr.port = endpoint.port();

        if(asio_addr.is_v4()){
            res_addr.type = Address::IPv4;
            res_addr.data = asio_addr.to_v4().to_uint();
        }else if(asio_addr.is_v6()){
            res_addr.type = Address::IPv6;
            res_addr.data = asio_addr.to_v6().to_bytes();
        }
    }

    return res_addr;
}

struct Server::Impl{
    asio::io_context context;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::vector<std::jthread> workers;

    asio::ip::tcp::acceptor acceptor;
    asio::ip::udp::socket udp_socket;

    asio::steady_timer retry_timer;

    Server& server;

    /// 保证调用线性
    asio::strand<asio::io_context::executor_type> strand;

    /// TCP存储
    std::pmr::unordered_map<int64_t,size_t> tcp_mapper;

    struct SocketWrapper{
        asio::ip::tcp::socket socket;
        bool valid;

        SocketWrapper(asio::ip::tcp::socket && soc)
        :socket(std::move(soc)){
            valid = true;
        }

        SocketWrapper(SocketWrapper && s)
        :socket(std::move(s.socket))
        ,valid(s.valid){}

        void reset(asio::ip::tcp::socket && soc){
            socket = std::move(soc);
            valid = true;
        }

        void d_cleanup(){
            socket.close();
            valid = false;
        }

        auto operator->(){return &socket;}

        operator bool(){return valid;}
    };

    // 这里就是假设tcp_sockets和established_clients是对齐的
    // 实际上由于分发策略一致 + strand保证原子化执行,就是对齐的
    ecs::detail::LinearStorage<SocketWrapper,std::pmr::unordered_map<size_t,SocketWrapper>> tcp_sockets;

    /// UDP存储
    std::optional<std::array<std::byte,65535>> udp_receive_buffer;
    asio::ip::udp::endpoint temp_remote_endpoint;
    std::pmr::unordered_map<asio::ip::udp::endpoint, size_t> udp_mapper;


    Impl(Server & sv,std::pmr::memory_resource * __a)
    :work_guard(asio::make_work_guard(context))
    ,acceptor(context)
    ,udp_socket(context)
    ,server(sv)
    ,retry_timer(context)
    ,udp_mapper(__a)
    ,tcp_sockets(0,__a,__a)
    ,tcp_mapper(__a)
    ,strand(context.get_executor()){}

    ~Impl(){
        retry_timer.cancel();
        acceptor.close();
        udp_socket.close();

        work_guard.reset();
        context.stop();
        workers.clear();
    }

    void handle_disconnect(size_t index,uint32_t token){
        // 安全,因为这没有更加深的调用
        {
            std::lock_guard<std::mutex> lock(server.mapper_lock);
            auto & cli = server.established_clients[index];
            if(cli.version != token)return;
            
            auto & soc = tcp_sockets[index];
            auto it = tcp_mapper.find(soc->native_handle());
            if(it != tcp_mapper.end())tcp_mapper.erase(it);

            if(!tcp_sockets.available_bits.get(index))tcp_sockets.remove(index);
            if(!server.established_clients.available_bits.get(index))server.established_clients.remove(index);
        }
    }

    void do_read_body(size_t index,size_t data_index,uint32_t token){
        auto& socket = tcp_sockets[index]; 
        auto& client = server.established_clients[index];
        if(client.version != token)return;

        auto& data = client.package_storage[data_index];

        /// 读取数据
        data.bytes.resize(data.expected_lentgh);
        asio::async_read(socket.socket,asio::buffer(data.bytes),
        asio::bind_executor(strand,[this,token,index,data_index](std::error_code ec,size_t length){
            if(!ec){
                auto& client = server.established_clients[index];
                // 说明都不是你该管的事情
                if(client.version != token)return;
                auto& data = client.package_storage[data_index];
                data.prepared = true;

                /// TCP保序
                {
                    /// 安全,这里粒度足够小
                    std::lock_guard<std::mutex> lock(server.data_lock);
                    server.pack_queue.push_back({index,std::move(data)});
                }
                server.semaphore.release();
                // 在这个模式下,packages不会被使用
                client.package_storage.remove(data_index);
                do_read_header(index);
            }else{
                handle_disconnect(index,token);
            }
        }));
    }
    
    void do_read_header(size_t index){
        auto& socket = tcp_sockets[index]; 
        auto& client = server.established_clients[index];

        uint32_t token = client.version;

        asio::async_read(socket.socket,asio::buffer(client.header_buffer),
            asio::bind_executor(strand, [this,token, index](std::error_code ec, size_t length){
                auto & client = server.established_clients[index];
                if(client.version != token)return;
                auto & socket = tcp_sockets[index];
                if(!ec){
                    auto & client = server.established_clients[index];
                    PackageHeader * header = reinterpret_cast<PackageHeader*>(client.header_buffer.data());
                    header->from_web();

                    if(header->length <= server.config.single_pack_max_size){
                        size_t data_index;
                        bool flag;
                        auto & data = client.package_storage.try_next_with_index(flag,data_index,server.res);
                        data.version_code = header->version_code;
                        data.category = static_cast<PackCategory>(header->category);
                        data.expected_lentgh = header->length;
                        // std::cout << "expected " << data.expected_lentgh << " smax" << server.config.single_pack_max_size << std::endl;
                        // if(server.config.strategy != server.config.Package)client.packages.push_back(data_index);
                        do_read_body(index,data_index,token);
                    }else{
                        invoke_error(
                            err_web_error,
                            "Client {} sended pack with estimated size {}![max:{}]",
                            socket->remote_endpoint().address().to_string(),
                            header->length,
                            server.config.single_pack_max_size
                        );
                        handle_disconnect(index,token);  
                    }
                }else{
                    handle_disconnect(index,token);
                }
            })
        );
    }

    void do_accept(){
        acceptor.async_accept(
        asio::bind_executor(strand,
            [this](std::error_code ec,asio::ip::tcp::socket socket){
            if(!ec){
                bool flag;
                size_t index;
                socket.set_option(asio::ip::tcp::no_delay(true));
                {
                    /// 安全,也没有设计更加深的调用
                    std::lock_guard<std::mutex> lock(server.mapper_lock);
                    server.established_clients.try_next_with_index(flag,index,server.res);
                    tcp_mapper.emplace(socket.native_handle(),index);
                    tcp_sockets.try_next_with_index(flag,index,std::move(socket));
                }

                do_read_header(index);
            }else if(server.config.report_internal_errors){
                // 要迅速,因此不采取fmt报错
                invoke_error(err_internal_web_error,ec.message());
            }
            if(ec.value() == EMFILE || ec.value() == ENFILE){
                retry_timer.expires_after(server.config.retry_time);
                retry_timer.async_wait([this](auto){do_accept();});
            }else do_accept();
        }));
    }

    void do_receive_from() {
        // udp_buffer 是一个预分配好的 std::array<std::byte, 65535>
        udp_socket.async_receive_from(
        asio::buffer(*udp_receive_buffer), temp_remote_endpoint,
        asio::bind_executor(strand,
            [this](std::error_code ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0) {
                // 1. 查找或创建 udp_mapper 中的 endpoint
                // 2. 将 udp_buffer 的内容包装成 Package
                // 3. 存入对应的 ClientPending
            }else if(server.config.report_internal_errors){
                // 要迅速,因此不采取fmt报错
                invoke_error(err_internal_web_error,ec.message());
            }
            do_receive_from();
        }));
    }
};

Server::Server(const ServerConfig & cfg,ProtocolType type,std::pmr::memory_resource * __a,size_t rsv)
:established_clients(rsv,__a,__a)
,pimpl(std::make_unique<Impl>(*this,__a))
,config(cfg)
,res(__a)
,pack_queue(__a)
,semaphore(0)
,protocol_type(type){
    __internal_setup();
    max_version = 0;
}
Server::~Server(){
    server_status = Stopped;
    // 唤醒足够多
    semaphore.release(10000);
}

bool Server::try_pop(DataPack & pack,size_t & client_id){
    /// 安全,没有更深的调用
    std::lock_guard<std::mutex> lock(data_lock);
    if(pack_queue.empty())return false;
    auto & p = pack_queue.front();
    pack = std::move(p.pack);
    client_id = p.client_id;
    pack_queue.pop_front();
    return true;
}

bool Server::__internal_start(){
    size_t sz = config.concurrent_workers <= 0 ? std::thread::hardware_concurrency()  : config.concurrent_workers;
    for(size_t i = 0; i < sz;++i){
        pimpl->workers.emplace_back([this](){
            try{
                pimpl->context.run(); 
            }catch(const std::exception& e){
                if(config.report_internal_errors){
                    invoke_error(err_internal_web_error,e.what());
                }
            }
        });
    }
    return true;
}

void Server::__internal_setup(){
    using namespace asio::ip;
    auto get_endpoint = [this]<class T>(T*){
        using endpoint_t = typename T::endpoint;
        endpoint_t endpoint;
        if(config.address.type == config.address.IPv4){
            endpoint = endpoint_t(
                address_v4(std::get<Address::v4_t>(config.address.data)),
                config.address.port
            );
        }else{
            endpoint = endpoint_t(
                address_v6(std::get<Address::v6_t>(config.address.data)),
                config.address.port
            );
        }
        return endpoint;
    };

    if(protocol_type == ProtocolType::TCP){
        auto endpoint = get_endpoint((tcp*)nullptr);

        pimpl->acceptor.open(endpoint.protocol());
        pimpl->acceptor.set_option(tcp::acceptor::reuse_address(true));
        pimpl->acceptor.bind(endpoint);
        pimpl->acceptor.listen();

        pimpl->do_accept();
    }else{
        auto endpoint = get_endpoint((udp*)nullptr);
    
        pimpl->udp_socket.open(endpoint.protocol());
        pimpl->udp_socket.bind(endpoint);
        pimpl->udp_receive_buffer.emplace();

        pimpl->do_receive_from();
    }
}
        
bool ALIB5_API Server::__internal_send(size_t client_id, const DataPack & pack){
    if(client_id >= established_clients.size()) return false;
    auto promise = std::make_shared<std::promise<std::error_code>>();
    auto future = promise->get_future();

    DataPack copy(res);
    copy.version_code = pack.version_code;
    copy.category = pack.category;
    copy.expected_lentgh = pack.expected_lentgh;
    copy.bytes.assign(pack.bytes.begin(), pack.bytes.end());    
    copy.callback = [promise](std::error_code ec) {
        promise->set_value(ec);
    };

    if(!__internal_async_send(client_id, std::move(copy))) {
        return false;
    }

    std::error_code result_ec = future.get(); 
    if(result_ec && config.report_internal_errors){
        invoke_error(err_internal_web_error, result_ec.message());
    }

    return !result_ec;
}

void Server::do_write_loop(size_t client_id){
    auto& client = established_clients[client_id];
    auto& sw = pimpl->tcp_sockets[client_id];

    if(!sw.valid){
        /// 安全,粒度很小
        std::lock_guard<std::mutex> lock(*client.queue_lock);
        client.write_queue.clear();
        client.is_writing = false;
        return;
    }

    DataPack* current_pack = nullptr;
    {
        /// 安全,不涉及深层调用
        std::lock_guard<std::mutex> lock(*client.queue_lock);
        if (client.write_queue.empty()) {
            client.is_writing = false;
            return;
        }
        current_pack = &client.write_queue.front();
    }

    PackageHeader & header = client.header_scratch;
    header.version_code = current_pack->version_code;
    header.category = static_cast<uint8_t>(current_pack->category);
    header.length = current_pack->expected_lentgh;
    header.to_web();

    std::array<asio::const_buffer, 2> buffers = {
        asio::buffer(&header, sizeof(header)),
        asio::buffer(current_pack->bytes.data(), current_pack->bytes.size())
    };

    uint32_t token = client.version;

    asio::async_write(sw.socket, buffers, 
        asio::bind_executor(pimpl->strand, [token,current_pack,this, client_id](std::error_code ec, size_t) {
            auto& client = established_clients[client_id];
            if(client.version != token)return;
            
            std::function<void(std::error_code)> cb;
            {
                /// 安全,这里也是不涉及调用
                std::lock_guard<std::mutex> lock(*client.queue_lock);
                if(!client.write_queue.empty()){
                    cb = std::move(client.write_queue.front().callback);
                    client.write_queue.pop_front();
                }
            }
            if(cb)cb(ec);

            if(!ec){
                do_write_loop(client_id);
            }else{
                pimpl->handle_disconnect(client_id,client.version);
            }
    }));
}

bool ALIB5_API Server::__internal_async_send(size_t client_id,DataPack && pack){
    if (client_id >= established_clients.size()) return false;
    auto& client = established_clients[client_id];
    bool expected_false = false;
    {
        /// 安全,没有深层次 
        std::lock_guard<std::mutex> lock(*client.queue_lock);
        client.write_queue.push_back(std::move(pack));
        if(client.is_writing){
            return true;
        }
        client.is_writing = true;
    }

    asio::post(pimpl->strand, [this, client_id](){
        do_write_loop(client_id);
    });

    return true;
}

struct Server::Client::Impl{
    // 共有数据为client_id

    /// 缓存的版本号
    uint32_t cached_version;

    /// TCP
    int64_t tcp_handle;
    asio::ip::tcp::endpoint tcp_endpoint;
    // size_t socket_loc; 这个理论上就是client_id

    /// UDP
    asio::ip::udp::endpoint udp_endpoint;
};

Server::Client::Client()
:pimpl(std::make_unique<Impl>()){}

Server::Client::~Client(){}

int64_t Server::get_client(size_t id,Server::Client & cli){
    {
        /// 安全,也是没有跳转
        std::lock_guard<std::mutex> lock(mapper_lock);
        if(id >= established_clients.size() || established_clients.available_bits.get(id)){
            // 说明这一个是free的也就是没有内容
            return err_gclient_not_found;
        }
        auto & client = established_clients[id];
        cli.pimpl->cached_version = client.version;
        if(protocol_type == ProtocolType::TCP){
            auto & socket = pimpl->tcp_sockets[id];
            if(socket){
                cli.pimpl->tcp_handle = socket->native_handle();
                std::error_code ec;
                cli.pimpl->tcp_endpoint = socket->remote_endpoint(ec);
                if(ec && config.report_internal_errors){
                    invoke_error(err_internal_web_error,ec.message());
                }
            }else{
                return err_gclient_not_found;
            }
        }else{
            /// @todo 这里可能需要反向mapper
        }
    }
    cli.client_id = id;
    cli.server = this;
    return err_success;
}