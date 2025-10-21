#include <iostream>
#include <signal.h>

#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/ThreadPool.h"

#include "http_handler.h"

std::map<uint32_t, HttpHandlerPtr> s_http_handler_map;

class HttpServer
{
public:
    HttpServer(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr, const std::string &name,  int num_event_loops
                ,int num_threads)
    :loop_(loop)
    , server_(loop, addr,name)
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
    std::atomic<uint32_t> conn_uuid_generator_ = 0;
    const int num_threads_ = 0;

    std::mutex mtx_;
};

// 实现回调函数
void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        std::cout << "New connection from " << conn->peerAddress().toIpPort() << std::endl;
        conn->send("Welcome to ChatRoom!\n");

        uint32_t uuid = conn_uuid_generator_++;
        conn->setContext(uuid);
        HttpHandlerPtr http_conn = std::make_shared<HttpHandler>(conn);
        std::lock_guard<std::mutex> ulock(mtx_);
        s_http_handler_map.insert({uuid, http_conn});

        
    }
    else
    {
        std::cout << "Connection closed: " << conn->peerAddress().toIpPort() << std::endl;
    }
}

void HttpServer::onMessage(const muduo::net::TcpConnectionPtr& conn, 
                          muduo::net::Buffer* buffer, 
                          muduo::Timestamp /*receiveTime*/)
{
    std::string message = buffer->retrieveAllAsString();
    std::cout << "Received from " << conn->peerAddress().toIpPort() 
              << ": " << message;
    
    // 回显消息给客户端
    conn->send("Echo: " + message);
}

void HttpServer::onWriteComplete(const muduo::net::TcpConnectionPtr& conn)
{
    std::cout << "Write complete for " << conn->peerAddress().toIpPort() << std::endl;
}




int main(int /*argc*/, char* /*argv*/[])
{
    signal(SIGPIPE, SIG_IGN);
    // int ret = 0;
    const char* str_conf = "conf.conf";
    (void)str_conf; // 避免未使用变量警告
    // if(argc > 1){
    //     str_conf = argv[1];
    // }else{
    //     str_conf = (char *)"conf.conf";
    // }
    
    // CConfigFileReader config_file(str_conf); 

    // 硬编码配置
    const char *http_bind_ip = "0.0.0.0";
    uint16_t http_bind_port = 8080;

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