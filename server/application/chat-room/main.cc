#include <iostream>
#include <signal.h>
#include <mutex>
#include <atomic>
#include <thread>

#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/ThreadPool.h"

#include "http_handler.h"
#include "config_file_reader.h"
#include "db_pool.h"
#include "cache_pool.h"
#include "pub_sub_service.h"
#include "api_msg.h"
#include "monitoring/metrics_collector.h"

#ifdef ENABLE_RPC
#include <grpcpp/grpcpp.h>
#include "comet_service.h"
#endif

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
    // std::cout << "Received from " << conn->peerAddress().toIpPort() 
    //           << ": " << message;
    
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
        pubSubService.AddRoomTopic(room.room_id, room.room_name, "1");
        LOG_INFO << "Added room to PubSubService: " << room.room_id << " - " << room.room_name;
    }
    
    return 0;
}

void check_redis_ready(CacheManager *cache_manager){
    // 检查两个连接池
    // 获取token连接池信息
    CacheConn *token_conn = cache_manager->GetCacheConn("token");
    if (token_conn) {
        LOG_INFO << "=== Token Redis连接信息 ===";
        LOG_INFO << "Pool Name: " << token_conn->GetPoolName();
        cache_manager->RelCacheConn(token_conn);
    }
    
    // 获取msg连接池信息  
    CacheConn *msg_conn = cache_manager->GetCacheConn("msg");
    if (msg_conn) {
        LOG_INFO << "=== Msg Redis连接信息 ===";
        LOG_INFO << "Pool Name: " << msg_conn->GetPoolName();
        cache_manager->RelCacheConn(msg_conn);
    }
    
    // 向Redis插入标记数据
    LOG_INFO << "=== 向Redis插入启动标记 ===";
    
    // 在token池中插入启动标记
    CacheConn *token_conn_mark = cache_manager->GetCacheConn("token");
    if (token_conn_mark) {
        string mark_key = "chatroom:server:startup";
        string mark_value = "server_started_at_" + std::to_string(time(nullptr));
        string result = token_conn_mark->Set(mark_key, mark_value);
        LOG_INFO << "Token Redis标记插入结果: " << result;
        LOG_INFO << "插入的标记: " << mark_key << " = " << mark_value;
        cache_manager->RelCacheConn(token_conn_mark);
    }
    
    // 在msg池中插入启动标记
    CacheConn *msg_conn_mark = cache_manager->GetCacheConn("msg");
    if (msg_conn_mark) {
        string msg_mark_key = "chatroom:msg:startup";
        string msg_mark_value = "msg_service_ready_" + std::to_string(time(nullptr));
        string result = msg_conn_mark->Set(msg_mark_key, msg_mark_value);
        LOG_INFO << "Msg Redis标记插入结果: " << result;
        LOG_INFO << "插入的标记: " << msg_mark_key << " = " << msg_mark_value;
        cache_manager->RelCacheConn(msg_conn_mark);
    }
    
    LOG_INFO << "=== Redis连接验证完成 ===";
    // 此时可以去redis里查看是否插入成功。
    // 因为我机器上有两套环境,防止混淆，加入这个check
}

// 定时持久化消息的回调函数
void on_persist_messages_timer(muduo::net::EventLoop* loop) {
    static int persist_counter = 0;
    persist_counter++;
    
    LOG_DEBUG << "开始第 " << persist_counter << " 次定时持久化消息";
    
    try {
        int persisted_count = ApiBatchPersistMessages(100); // 每次最多处理100条消息
        if (persisted_count > 0) {
            LOG_INFO << "成功持久化 " << persisted_count << " 条消息到MySQL";
        } else if (persisted_count == 0) {
            LOG_DEBUG << "没有待持久化的消息";
        } else {
            LOG_ERROR << "持久化消息失败";
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "持久化消息异常: " << e.what();
    }
    
    // 设置下次定时器（10秒后再次执行）
    loop->runAfter(10.0, std::bind(&on_persist_messages_timer, loop));
}

// 启动消息持久化定时器
void start_message_persistence_timer(muduo::net::EventLoop* loop) {
    LOG_INFO << "启动消息持久化定时器，每10秒执行一次";
    // 延迟5秒开始第一次执行，给系统一些启动时间
    loop->runAfter(5.0, std::bind(&on_persist_messages_timer, loop));
}

void check_mysql_ready(CDBManager *db_manager){
     // 检查MySQL数据库连接并显示数据库列表
    LOG_INFO << "=== 检查MySQL数据库连接 ===";
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_slave");
    if (db_conn) {
        // 执行DESC users命令检查表结构
        LOG_INFO << "执行 DESC users 命令检查表结构...";
        const char* descSQL = "DESC users";
        CResultSet *desc_result = db_conn->ExecuteQuery(descSQL);
        if (desc_result) {
            LOG_INFO << "DESC users 执行成功，users表结构：";
            int count = 0;
            while (desc_result->Next()) {
                count++;
                // DESC 返回多列：Field, Type, Null, Key, Default, Extra
                const char* field = desc_result->GetString("Field");  // 字段名
                const char* type = desc_result->GetString("Type");   // 类型
                const char* null_able = desc_result->GetString("Null"); // 是否为NULL
                
                LOG_INFO << "字段 " << count << ":";
                if (field && strlen(field) > 0) {
                    LOG_INFO << "  Field: " << field;
                } else {
                    LOG_INFO << "  Field: [NULL]";
                }
                
                if (type && strlen(type) > 0) {
                    LOG_INFO << "  Type: " << type;
                } else {
                    LOG_INFO << "  Type: [NULL]";
                }
                
                if (null_able && strlen(null_able) > 0) {
                    LOG_INFO << "  Null: " << null_able;
                } else {
                    LOG_INFO << "  Null: [NULL]";
                }
            }
            LOG_INFO << "users表共有 " << count << " 个字段";
            delete desc_result;
        } else {
            LOG_ERROR << "执行 DESC users 失败，可能表不存在";
        }
        
        db_manager->RelDBConn(db_conn);
        LOG_INFO << "=== MySQL数据库检查完成 ===";
    } else {
        LOG_ERROR << "获取MySQL连接失败";
    }
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
    if (str_log_level && strlen(str_log_level) > 0) {
        muduo::Logger::LogLevel log_level = static_cast<muduo::Logger::LogLevel>(atoi(str_log_level));
        muduo::Logger::setLogLevel(log_level);
    } else {
        LOG_WARN << "log_level not configured, using default";
    }

    const char *http_bind_ip = "0.0.0.0";
    uint16_t http_bind_port = 8081;
    char *str_http_bind_port = config_file.GetConfigName("http_bind_port");
    if (str_http_bind_port && strlen(str_http_bind_port) > 0) {
        http_bind_port = atoi(str_http_bind_port);
    } else {
        LOG_WARN << "http_bind_port not configured, using default: " << http_bind_port;
    }

    // 初始化redis连接池，新建了token和msg连接池
    CacheManager::SetConfPath(str_conf);
    CacheManager *cache_manager = CacheManager::getInstance();
    
    if (!cache_manager) {
        LOG_ERROR <<"CacheManager init failed";
        return -1;
    }

    // check_redis_ready(cache_manager);

    // 初始化mysql连接池
    CDBManager::SetConfPath(str_conf);   //设置配置文件路径
    CDBManager *db_manager = CDBManager::getInstance();
    if (!db_manager) {
        LOG_ERROR <<"DBManager init failed";
        return -1;
    }

    // check_mysql_ready(db_manager);

    // 初始化监控系统
    uint16_t metrics_port = 9091;  // Comet metrics 端口
    char *str_metrics_port = config_file.GetConfigName("metrics_port");
    if (str_metrics_port && strlen(str_metrics_port) > 0) {
        metrics_port = atoi(str_metrics_port);
    }
    
    std::string metrics_bind_address = std::string("0.0.0.0:") + std::to_string(metrics_port);
    MetricsCollector::GetInstance().Initialize(metrics_bind_address, "comet");
    LOG_INFO << "Metrics endpoint initialized at http://" << metrics_bind_address << "/metrics";

    int num_event_loops = 0; 
    int num_threads = 0;
    // int timeout_ms = 10;

    muduo::net::EventLoop loop; 
    muduo::net::InetAddress addr(http_bind_ip, http_bind_port);

    HttpServer server(&loop, addr, "HttpServer", num_event_loops, num_threads);

    // 启动消息持久化定时器
    start_message_persistence_timer(&loop);
    
#ifdef ENABLE_RPC
    // 启动 gRPC 服务器
    std::string grpc_server_address("0.0.0.0:50051");
    
    // 从配置文件读取 gRPC 端口（可选）
    char *str_grpc_port = config_file.GetConfigName("grpc_port");
    if (str_grpc_port && strlen(str_grpc_port) > 0) {
        grpc_server_address = std::string("0.0.0.0:") + str_grpc_port;
    }
    
    ChatRoom::CometServiceImpl grpc_service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(grpc_server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&grpc_service);
    
    std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
    LOG_INFO << "gRPC Server 启动成功，监听地址: " << grpc_server_address;
    
    // 在单独的线程中运行 gRPC Server
    std::thread grpc_thread([&grpc_server]() {
        grpc_server->Wait();
    });
#endif
    
    server.start();
    LOG_INFO << "服务器启动完成，监听地址: " << http_bind_ip << ":" << http_bind_port;
    LOG_INFO << "消息持久化定时器已启动";
    
    loop.loop(); 

#ifdef ENABLE_RPC
    // 关闭 gRPC 服务器
    grpc_server->Shutdown();
    if (grpc_thread.joinable()) {
        grpc_thread.join();
    }
#endif

    return 0;
}