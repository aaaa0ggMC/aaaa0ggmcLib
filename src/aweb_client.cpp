#include <alib5/aweb.h>
#include <asio.hpp>
#include <thread>

using namespace alib5;
using namespace alib5::web;

struct Client::Impl{
    asio::io_context context;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::vector<std::jthread> workers;

    asio::strand<asio::io_context::executor_type> strand;

    /// TCP,socket中就可以获取endpoint了
    asio::ip::tcp::socket tcp_socket;

    /// UDP
    asio::ip::udp::socket udp_socket;
    /// 写入队列
    PackageHeader write_header_scratch;
    std::mutex queue_lock;
    bool is_writing { false };
    std::pmr::deque<DataPack> write_queue;

    /// 读取队列
    PackageHeader header_scratch;
    ecs::detail::LinearStorage<DataPack> packs;
    // 目前还没打算做partial receive,暂时没做
    /// @todo 完善这个?
    std::pmr::deque<size_t> pack_indices;

    Client * client;
    Impl(Client & c)
    :work_guard(asio::make_work_guard(context))
    ,client(&c)
    ,tcp_socket(context)
    ,udp_socket(context)
    ,strand(context.get_executor()){

    }

    void do_write_loop(){
        DataPack* current_pack = nullptr;
        {
            std::lock_guard<std::mutex> lock(queue_lock);
            if(write_queue.empty()){
                is_writing = false;
                return;
            }
            current_pack = &write_queue.front();
        }
        write_header_scratch.version_code = current_pack->version_code;
        write_header_scratch.category = static_cast<uint8_t>(current_pack->category);
        write_header_scratch.length = current_pack->expected_lentgh;
        write_header_scratch.to_web();

        std::array<asio::const_buffer, 2> buffers = {
            asio::buffer(&write_header_scratch, sizeof(PackageHeader)),
            asio::buffer(current_pack->bytes.data(), current_pack->bytes.size())
        };

        asio::async_write(tcp_socket, buffers, 
            asio::bind_executor(strand, [this](std::error_code ec, size_t){
                std::function<void(std::error_code)> fn;
                {
                    std::lock_guard<std::mutex> lock(queue_lock);
                    if(write_queue.front().callback){
                        fn = std::move(write_queue.front().callback);
                    }
                    write_queue.pop_front();
                }
                if(fn){
                    fn(ec);
                }

                if(!ec){
                    do_write_loop();
                }else{
                    client->disconnect();
                }
            })
        );
    }

    void do_read_header(){
        asio::async_read(tcp_socket, asio::buffer(&header_scratch, sizeof(PackageHeader)),
            asio::bind_executor(strand, [this](std::error_code ec, size_t length){
                if(!ec){
                    
                    header_scratch.from_web();
                    if(header_scratch.length <= client->config.single_pack_max_size){
                        // 准备一个 DataPack
                        bool flag;
                        size_t index;
                        auto& pack = packs.try_next_with_index(flag, index, client->res);
                        pack.version_code = header_scratch.version_code;
                        pack.category = static_cast<PackCategory>(header_scratch.category);
                        pack.expected_lentgh = header_scratch.length;
                        pack.bytes.resize(header_scratch.length);
                        
                        do_read_body(index);
                    }else{
                        if(client->config.report_internal_errors)
                            invoke_error(err_web_error, "Packet size exceeds limit");
                        client->disconnect();
                    }
                }else{
                    // 这里的错误通常意味着连接断开
                    client->disconnect();
                    client->client_status = Disconnected;
                }
            })
        );
    }

    void do_read_body(size_t index){
        auto & pack = packs[index];

        asio::async_read(tcp_socket, asio::buffer(pack.bytes),
            asio::bind_executor(strand, [this, index](std::error_code ec, size_t length) {
                if (!ec) {
                    auto& pack = packs[index];
                    pack.prepared = true;
                    
                    // TCP保序
                    {
                        std::lock_guard<std::mutex> lock(client->data_lock);
                        client->queue.push_back(std::move(pack));
                    }
                    client->semaphore.release();
                    packs.remove(index);

                    do_read_header();
                } else {
                    packs.remove(index);
                    client->client_status = Disconnected;
                }
            })
        );
    }
};

Client::Client(ClientConfig cfg,ProtocolType type,std::pmr::memory_resource * __a)
:protocol_type(type)
,res(__a)
,pimpl(std::make_unique<Impl>(*this))
,config(cfg)
,semaphore(0)
,queue(__a){
    max_version = 0;
}

Client::~Client(){
    disconnect();
}

int64_t Client::disconnect(){
    if(client_status == Disconnected) return err_success;
    client_status = Disconnected;

    std::error_code ec;
    if(pimpl->tcp_socket.is_open()){
        auto a = pimpl->tcp_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        a = pimpl->tcp_socket.close(ec);
    }
    if (pimpl->udp_socket.is_open()) {
        auto a = pimpl->udp_socket.close(ec);
    }
    pimpl->context.stop();
    pimpl->workers.clear();

    pimpl->context.restart(); 

    {
        std::lock_guard<std::mutex> lock(data_lock);
        queue.clear();
        pimpl->packs.clear(); 
    }
    {
        std::lock_guard<std::mutex> lock(pimpl->queue_lock);
        pimpl->write_queue.clear();
        pimpl->is_writing = false;
    }

    semaphore.release(65536);
    return err_success;
}

int64_t Client::connect(const Address& addr){
    auto promise = std::make_shared<std::promise<std::error_code>>();
    auto future = promise->get_future();
    int64_t res = async_connect(addr, [promise](std::error_code ec) {
        promise->set_value(ec);
    });

    if(res != err_success)return res;

    std::error_code ec = future.get();
    return ec ? err_client_disconnected : err_success;
}

int64_t Client::async_connect(const Address& addr,std::function<void(std::error_code)> fin_callback){
    if(client_status == Connected){
        return err_client_connected;
    }

    using namespace asio::ip;
    auto get_endpoint = [this,&addr]<class T>(T*){
        using endpoint_t = typename T::endpoint;
        endpoint_t endpoint;
        if(addr.type == addr.IPv4){
            endpoint = endpoint_t(
                address_v4(std::get<Address::v4_t>(addr.data)),
                addr.port
            );
        }else{
            endpoint = endpoint_t(
                address_v6(std::get<Address::v6_t>(addr.data)),
                addr.port
            );
        }
        return endpoint;
    };

    while(semaphore.try_acquire())continue;

    if(protocol_type == ProtocolType::TCP){
        if(!pimpl->tcp_socket.is_open()){
            pimpl->tcp_socket.open(addr.type == Address::IPv4 ? tcp::v4() : tcp::v6());
        }

        pimpl->tcp_socket.async_connect(get_endpoint((tcp*)nullptr),
        asio::bind_executor(pimpl->strand,
            [fn = std::move(fin_callback),this](std::error_code ec){
                if(!ec){
                    pimpl->tcp_socket.set_option(asio::ip::tcp::no_delay(true));
                    client_status = Connected;
                    if(fn)fn(ec);

                    pimpl->do_read_header();
                }else{
                    client_status = Disconnected;
                    if(fn)fn(ec);
                    if(config.report_internal_errors)
                        invoke_error(err_internal_web_error,"Connect failed: {}",ec.message());
                }
            }
        )
    );
    }else{
        // pimpl->udp_socket.connect(get_endpoint((udp*)nullptr));
    }

    if(pimpl->context.stopped()) pimpl->context.restart();
    for(size_t i = 0;i < config.concurrent_workers;++i){
        pimpl->workers.emplace_back(
            [this]{
                try{
                    pimpl->context.run(); 
                }catch(const std::exception& e){
                    if(config.report_internal_errors){
                        invoke_error(err_internal_web_error,e.what());
                    }
                }
            }
        );
    }

    return err_success;
}

int64_t Client::peek(DataPack & pack){
    std::lock_guard<std::mutex> lock(data_lock);
    if(queue.empty())return err_client_empty_queue;

    // 移动语义，零拷贝
    pack = std::move(queue.front());
    queue.pop_front();
    return err_success;
}

int64_t Client::receive(DataPack & pack){
    while(true){
        if(peek(pack) == err_success) return err_success;
 
        if(client_status == Disconnected){
            return err_client_disconnected; 
        }

        semaphore.acquire();
    }
}

bool Client::__internal_send(const DataPack & pack) {
    if(client_status != Connected) return false;
    auto sent_promise = std::make_shared<std::promise<std::error_code>>();
    auto sent_future = sent_promise->get_future();

    DataPack copy(res);
    copy.version_code = pack.version_code;
    copy.category = pack.category;
    copy.expected_lentgh = pack.expected_lentgh;
    copy.bytes.assign(pack.bytes.begin(), pack.bytes.end());

    copy.callback = [sent_promise](std::error_code ec) {
        sent_promise->set_value(ec);
    };

    if(!__internal_async_send(std::move(copy))){
        return false;
    }

    std::error_code result_ec = sent_future.get();
    return !result_ec;
}

bool Client::__internal_async_send(DataPack && pack){
    if(client_status != Connected)return false;

    {
        std::lock_guard<std::mutex> lock(pimpl->queue_lock);
        pimpl->write_queue.push_back(std::move(pack));
        if (pimpl->is_writing) {
            return true;
        }
        pimpl->is_writing = true;
    }

    asio::post(pimpl->strand, [this](){
        pimpl->do_write_loop();
    });

    return true;
}