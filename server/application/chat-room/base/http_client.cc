#include "http_client.h"
#include <sstream>

HttpClient::HttpClient(EventLoop* loop)
    : loop_(loop),
      connected_(false) {
}

HttpClient::~HttpClient() {
}

void HttpClient::AsyncPost(const std::string& host,
                          int port,
                          const std::string& path,
                          const std::string& json_body,
                          ResponseCallback callback) {
    callback_ = callback;
    
    // 构造 HTTP POST 请求
    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << ":" << port << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << json_body.size() << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << json_body;
    
    pending_request_ = request.str();
    
    // 创建 TCP 客户端连接
    InetAddress serverAddr(host, port);
    client_.reset(new TcpClient(loop_, serverAddr, "HttpClient"));
    
    client_->setConnectionCallback(
        std::bind(&HttpClient::onConnection, this, std::placeholders::_1));
    client_->setMessageCallback(
        std::bind(&HttpClient::onMessage, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));
    
    client_->connect();
}

void HttpClient::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        connected_ = true;
        LOG_DEBUG << "Connected to Logic service, sending request";
        conn->send(pending_request_);
    } else {
        connected_ = false;
        LOG_WARN << "Disconnected from Logic service";
    }
}

void HttpClient::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
    std::string response = buf->retrieveAllAsString();
    
    // 简单解析 HTTP 响应（检查状态码）
    bool success = (response.find("HTTP/1.1 200 OK") != std::string::npos);
    
    if (callback_) {
        callback_(success, response);
    }
    
    // 关闭连接
    conn->shutdown();
}
