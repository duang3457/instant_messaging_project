#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <functional>
#include <memory>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

using namespace muduo;
using namespace muduo::net;

// 简单的异步 HTTP 客户端，用于发送 POST 请求到 Logic 服务
class HttpClient {
public:
    using ResponseCallback = std::function<void(bool success, const std::string& response)>;

    HttpClient(EventLoop* loop);
    ~HttpClient();

    // 异步发送 POST 请求
    void AsyncPost(const std::string& host, 
                   int port,
                   const std::string& path,
                   const std::string& json_body,
                   ResponseCallback callback);

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);

    EventLoop* loop_;
    std::unique_ptr<TcpClient> client_;
    ResponseCallback callback_;
    std::string pending_request_;
    bool connected_;
};

#endif // HTTP_CLIENT_H
