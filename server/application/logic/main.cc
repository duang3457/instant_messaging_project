#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <functional>
#include <string>
#include <map>
#include <jsoncpp/json/json.h>

#include "kafka_producer.h"
#include "ChatRoom.Job.pb.h"
#include "http_parser.h"
using namespace muduo;
using namespace muduo::net;
// 封装 JSON 字符串为 HTTP 响应
std::string wrapJsonInHttpResponse(const std::string& jsonContent) {
    std::string httpResponse = "HTTP/1.1 200 OK\r\n";
    httpResponse += "Content-Type: application/json\r\n";
    httpResponse += "Content-Length: " + std::to_string(jsonContent.size()) + "\r\n";
    httpResponse += "\r\n";
    httpResponse += jsonContent;
    return httpResponse;
}
class HttpServer {
public:
    HttpServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "HttpServer"),
          loop_(loop) {
        server_.setConnectionCallback(
            std::bind(&HttpServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, _1, _2, _3));
        server_.setThreadNum(1);     
        // 初始化Kafka连接
        if (!producer_.init("localhost:9092", "my-topic")) {
            LOG_ERROR << "Failed to initialize Kafka producer";
        }
    }

    ~HttpServer() {
        producer_.close();  // 确保关闭Kafka连接
    }

    void start() {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO << "New connection from " << conn->peerAddress().toIpPort();
        } else {
            LOG_INFO << "Connection closed from " << conn->peerAddress().toIpPort();
        }
    }

    void onMessage(const TcpConnectionPtr& conn,
                  Buffer* buf,
                  Timestamp receiveTime) {
        string msg = buf->retrieveAllAsString();
        LOG_INFO << "Received message: " << msg;
        // 解析HTTP请求
        if (msg.find("POST /logic/login") != string::npos) {
            handleLogin(conn, msg);
        }
        else if (msg.find("POST /logic/send") != string::npos) {
            handleSend(conn, msg);
        }
        else {
            // 返回404
            string response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Length: 0\r\n\r\n";
            conn->send(response);
        }
    }

    void handleLogin(const TcpConnectionPtr& conn, const string& request) {
        // 简单的登录处理
        string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: 25\r\n\r\n";
        response += "{\"status\": \"logged in\"}\r\n";
        conn->send(response);
    }

    void handleSend(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            return;
        }

        // 解析JSON body
        Json::Value json;
        if (!HttpParser::parseJsonBody(body, json)) {
            LOG_ERROR << "Failed to parse JSON body";
            return;
        }

        // 验证必要字段
        if (!json.isMember("roomId") || !json.isMember("userId") || 
            !json.isMember("userName") || !json.isMember("messages")) {
            LOG_ERROR << "Missing required fields in request";
            return;
        }

        // 创建并填充PushMsg消息
        ChatRoom::Job::PushMsg pushMsg;  //protobuf
        pushMsg.set_type(ChatRoom::Job::PushMsg_Type_ROOM);
        pushMsg.set_operation(4);  // 4 代表发送消息
        pushMsg.set_room(json["roomId"].asString());

        // 构造新的消息格式
        Json::Value messageWrapper;
        messageWrapper["type"] = "serverMessages";
        
        Json::Value payload;
        payload["roomId"] = json["roomId"].asString();
        
        Json::Value messagesArray(Json::arrayValue);
         // 遍历所有消息，将它们添加到同一个消息数组中
        for (const auto& message : json["messages"]) {
            if (!message.isMember("content")) {
                LOG_ERROR << "Message missing content field";
                continue;
            }

            Json::Value messageObj;
            // 生成消息ID: 时间戳-索引
            std::string messageId = std::to_string(Timestamp::now().microSecondsSinceEpoch() / 1000) + "-0";
            messageObj["id"] = messageId;
            messageObj["content"] = message["content"].asString();
            
            Json::Value userObj;
            userObj["id"] = json["userId"].asInt();
            userObj["username"] = json["userName"].asString();
            messageObj["user"] = userObj;
            
            // 设置时间戳（毫秒）
            messageObj["timestamp"] = (Json::UInt64)(Timestamp::now().microSecondsSinceEpoch() / 1000);
            
            messagesArray.append(messageObj);
        }

        payload["messages"] = messagesArray;
        messageWrapper["payload"] = payload;

        // 将整个消息对象序列化并设置到protobuf消息中
        Json::FastWriter writer;
        Json::StreamWriterBuilder writerBuilder;
        std::string jsonString =  writer.write(messageWrapper); 
        LOG_INFO << "jsonString: " << jsonString;
        pushMsg.set_msg(jsonString);

        // 序列化protobuf消息
        std::string serialized_msg;
        if (!pushMsg.SerializeToString(&serialized_msg)) {
            LOG_ERROR << "Failed to serialize message";
            string response = "HTTP/1.1 500 Internal Server Error\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: 31\r\n\r\n";
            response += "{\"status\": \"send failed\"}\r\n";
            conn->send(response);
            LOG_ERROR << "Failed to serialize message";
            return;
        }
        LOG_INFO << "serialized_msg: " << serialized_msg;
        // 发送序列化后的消息到Kafka
        if (!producer_.sendMessage(serialized_msg)) {
            LOG_ERROR << "Failed to send message to Kafka";
            string response = "HTTP/1.1 500 Internal Server Error\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: 31\r\n\r\n";
            response += "{\"status\": \"send failed\"}\r\n";
            conn->send(response);
            return;
        }

        // 发送成功响应
        // string response = "HTTP/1.1 200 OK\r\n";
        // response += "Content-Type: application/json\r\n";
        // response += "Content-Length: 29\r\n\r\n";
        // response += "{\"status\": \"message sent\"}\r\n";
        // LOG_INFO << "send   into";

        std::string jsonStr = "{\"status\": \"success\"}";
        std::string httpResponse = wrapJsonInHttpResponse(jsonStr);

        conn->send(httpResponse);
        LOG_INFO << "handleSend   successfully";
        conn->shutdown();
    }

    TcpServer server_;
    EventLoop* loop_;
    std::map<string, bool> loggedInUsers_; // 存储已登录用户
    KafkaProducer producer_;  // 添加 producer 成员变量
};

int main() {
    // 创建producer实例
    KafkaProducer producer;

    LOG_INFO << "HTTP server starting...";
    EventLoop loop;
    InetAddress listenAddr(8090);
    HttpServer server(&loop, listenAddr);
    server.start();
    loop.loop();
    return 0;
}
