#include <iostream>
#include <signal.h>
#include <mutex>
#include <atomic>

#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/ThreadPool.h"

#include "http_handler.h"
#include "config_file_reader.h"
#include "db_pool.h"
#include "cache_pool.h"
#include "pub_sub_service.h"

std::map<uint32_t, HttpHandlerPtr> s_http_handler_map;

class HttpServer
{
public:
    HttpServer(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr, const std::string &name,  int num_event_loops
                ,int num_threads)
    :loop_(loop)
    , server_(loop, addr,name)
    , conn_uuid_generator_(0)
    , num_threads_(num_threads)
    {
        server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        server_.setWriteCompleteCallback(std::bind(&HttpServer::onWriteComplete, this, std::placeholders::_1));
   
        server_.setThreadNum(num_event_loops);
    }

    void start(){
        if(num_threads_ != 0) thread_pool_.start(num_threads_);
        server_.start();
    }


private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp receiveTime);
    void onWriteComplete(const muduo::net::TcpConnectionPtr& conn);

    muduo::net::EventLoop *loop_ = nullptr;
    muduo::net::TcpServer server_;
    muduo::ThreadPool thread_pool_;
    std::atomic<uint32_t> conn_uuid_generator_;
    const int num_threads_ = 0;

    std::mutex mtx_;
};

// 实现回调函数
void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        uint32_t uuid = conn_uuid_generator_++;
        LOG_INFO << "uuid : " << uuid;
        conn->setContext(uuid);
        HttpHandlerPtr http_conn = std::make_shared<HttpHandler>(conn);

        std::lock_guard<std::mutex> ulock(mtx_);
        s_http_handler_map.insert({uuid, http_conn});
    }
    else
    {
        try {
            if (!conn->getContext().empty()) {
                uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
                LOG_INFO << "onConnection中" << "uuid: " << uuid << ", onConnection dis conn" << conn.get();
                std::lock_guard<std::mutex> ulock(mtx_); 
                s_http_handler_map.erase(uuid);  //自动释放对应http_handler
            } else {
                LOG_WARN << "Connection context is empty during disconnect";
            }
        } catch (const std::bad_any_cast& e) {
            LOG_ERROR << "Bad any_cast in onConnection disconnect: " << e.what();
        } catch (const std::exception& e) {
            LOG_ERROR << "Exception in onConnection disconnect: " << e.what();
        }
    }
}

void HttpServer::onMessage(const muduo::net::TcpConnectionPtr& conn, 
                          muduo::net::Buffer* buffer, 
                          muduo::Timestamp /*receiveTime*/)
{
    std::string message(buffer->peek(), buffer->readableBytes());
    std::cout << "Received from " << conn->peerAddress().toIpPort() 
              << ": " << message;
    
    // 检查连接状态
    if (!conn->connected()) {
        LOG_WARN << "Connection not connected, ignoring message";
        buffer->retrieveAll(); // 清空buffer
        return;
    }
    
    // 安全地获取 UUID
    uint32_t uuid = 0;
    try {
        if (!conn->getContext().empty()) {
            uuid = boost::any_cast<uint32_t>(conn->getContext());
        } else {
            LOG_ERROR << "Connection context is empty";
            buffer->retrieveAll(); // 清空buffer
            return;
        }
    } catch (const boost::bad_any_cast& e) {
        LOG_ERROR << "Bad any_cast when getting UUID: " << e.what();
        buffer->retrieveAll(); // 清空buffer
        return;
    }
    
    mtx_.lock();  
    // 这里不能是 &http_conn，否则不会有引用计数+1
    HttpHandlerPtr http_conn = s_http_handler_map[uuid];
    mtx_.unlock();
    
    // 检查连接处理器是否存在
    if (!http_conn) {
        LOG_ERROR << "No handler found for UUID: " << uuid;
        buffer->retrieveAll(); // 清空buffer
        return;
    }
    
    //处理 相关业务
    try {
        if(num_threads_ != 0)  //开启了线程池
            thread_pool_.run(std::bind(&HttpHandler::OnRead, http_conn, buffer)); //给到业务线程处理
        else {  //没有开启线程池
            http_conn->OnRead(buffer);  // 直接在io线程处理
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Exception in onMessage: " << e.what();
        buffer->retrieveAll(); // 清空buffer
    }
}

void HttpServer::onWriteComplete(const muduo::net::TcpConnectionPtr& /*conn*/)
{
    // std::cout << "Write complete for " << conn->peerAddress().toIpPort() << std::endl;
}

int load_room_list() {
    PubSubService& pubSubService = PubSubService::GetInstance();

    std::vector<Room> &all_rooms = PubSubService::GetRoomList(); //获取缺省的聊天室列表
     
    for (const auto& room : all_rooms) {
        pubSubService.AddRoomTopic(room.room_id, room.room_name, 1);
        LOG_INFO << "Added room to PubSubService: " << room.room_id << " - " << room.room_name;
    }
    
    return 0;
}

int main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);
    const char* str_conf = "conf.conf";
    if(argc > 1){
        str_conf = argv[1];
    }else{
        str_conf = (char *)"conf.conf";
    }

    load_room_list();
    
    CConfigFileReader config_file(str_conf); 

    char *str_log_level =  config_file.GetConfigName("log_level");  
    muduo::Logger::LogLevel log_level = static_cast<muduo::Logger::LogLevel>(atoi(str_log_level));
    muduo::Logger::setLogLevel(log_level);

    const char *http_bind_ip = "0.0.0.0";
    uint16_t http_bind_port = 8081;
    char *str_http_bind_port = config_file.GetConfigName("http_bind_port");  
    http_bind_port = atoi(str_http_bind_port);

    // 初始化redis连接池
    CacheManager::SetConfPath(str_conf);
    CacheManager *cache_manager = CacheManager::getInstance();
    if (!cache_manager) {
        LOG_ERROR <<"CacheManager init failed";
        return -1;
    }

    // 初始化mysql连接池
    CDBManager::SetConfPath(str_conf);   //设置配置文件路径
    CDBManager *db_manager = CDBManager::getInstance();
    if (!db_manager) {
        LOG_ERROR <<"DBManager init failed";
        return -1;
    }

    int num_event_loops = 0; 
    int num_threads = 0;
    // int timeout_ms = 10;

    muduo::net::EventLoop loop; 
    muduo::net::InetAddress addr(http_bind_ip, http_bind_port);

    HttpServer server(&loop, addr, "HttpServer", num_event_loops, num_threads);

    server.start();
    loop.loop(); 

    return 0;
}