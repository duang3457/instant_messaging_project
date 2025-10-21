#ifndef __HTTP_HANDLER_H__
#define __HTTP_HANDLER_H__

#include <unordered_map>
#include <cctype>
#include <sstream>

#include <muduo/net/TcpConnection.h>

#include <http_conn.h>

class CHttpConn;
class CWebSocketConn;

class HttpHandler {
public:
    enum RequestType {
        UNKNOWN,
        HTTP,
        WEBSOCKET
    };
    HttpHandler(const muduo::net::TcpConnectionPtr& conn): tcp_conn_(conn){}

    ~HttpHandler(){
        handler_.reset();
    }

    void OnRead(muduo::net::Buffer* buf){
        if(request_type_ = UNKNOWN) {
            const char* in_buf = buf->peek();
            int32_t len = buf->readableBytes();
            std::cout << "in_buf: " << in_buf << std::endl;

            auto headers = parseHttpHeaders(in_buf, len);
            if(isWebSocketRequest(headers)){
                request_type_ = WEBSOCKET;
                handler_ = std::make_shared<CWebSocketConn>(tcp_conn_);
                handler_->setHeaders(headers);
            }else{
                request_type_ = HTTP;
                handler_ = std::make_shared<CHttpConn>(tcp_conn_);
                handler_->setHeaders(headers);
            }
        }
        handler_->OnRead(buf);
    }

private:
    std::unordered_map<std::string, std::string> parseHttpHeaders(const char *data, int size) {
        std::string request(data, size);
        return parseHttpHeaders(request);
    }

    std::unordered_map<std::string, std::string> parseHttpHeaders(const std::string& request) {
        std::unordered_map<std::string, std::string> headers;
        std::istringstream stream(request);
        std::string line;

        while (std::getline(stream, line) && line != "\r") {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2, line.size() - colon - 3); // 去掉 ": " 和 "\r"
                headers[key] = value;
            }
        }
        for(const auto& [key, value] : headers){
            std::cout << key << ": " << value << std::endl;
        }
        return headers;
    }

    bool isWebSocketRequest(const std::unordered_map<std::string, std::string>& headers) {
        auto upgradeIt = headers.find("Upgrade");
        auto connectionIt = headers.find("Connection");

        if(upgradeIt != headers.end() && connectionIt != headers.end()){
            std::string upgrade = upgradeIt->second;
            std::string connection = connectionIt->second;

            std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);
            std::transform(connection.begin(), connection.end(), connection.begin(), ::tolower);

            return upgrade == "websocket" && connection.find("upgrade") != std::string::npos;
        }
        return false;
    }

    CHttpConnPtr handler_;
    muduo::net::TcpConnectionPtr tcp_conn_;
    RequestType request_type_ = UNKNOWN;
};

using HttpHandlerPtr = std::shared_ptr<HttpHandler>;

#endif