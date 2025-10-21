#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/InetAddress.h"
#include "muduo/base/Logging.h"

class ChatClient
{
public:
    ChatClient(muduo::net::EventLoop* loop, const muduo::net::InetAddress& serverAddr, const std::string& name)
        : loop_(loop)
        , client_(loop, serverAddr, name)
        , connected_(false)
    {
        client_.setConnectionCallback(
            std::bind(&ChatClient::onConnection, this, std::placeholders::_1));
        client_.setMessageCallback(
            std::bind(&ChatClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
        // 启用重连
        client_.enableRetry();
    }

    void connect()
    {
        client_.connect();
    }

    void disconnect()
    {
        client_.disconnect();
    }

    void sendMessage(const std::string& message)
    {
        if (connection_)
        {
            connection_->send(message + "\n");
        }
        else
        {
            std::cout << "Not connected to server!" << std::endl;
        }
    }

    bool isConnected() const
    {
        return connected_;
    }

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            std::cout << "Connected to server: " << conn->peerAddress().toIpPort() << std::endl;
            connected_ = true;
            connection_ = conn;
        }
        else
        {
            std::cout << "Disconnected from server" << std::endl;
            connected_ = false;
            connection_.reset();
        }
    }

    void onMessage(const muduo::net::TcpConnectionPtr& /*conn*/,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp /*time*/)
    {
        std::string message = buf->retrieveAllAsString();
        std::cout << "Server: " << message;
    }

    muduo::net::EventLoop* loop_;
    muduo::net::TcpClient client_;
    muduo::net::TcpConnectionPtr connection_;
    std::atomic<bool> connected_;
};

void inputThread(ChatClient* client)
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "quit" || line == "exit")
        {
            client->disconnect();
            break;
        }
        
        if (client->isConnected())
        {
            client->sendMessage(line);
        }
        else
        {
            std::cout << "Not connected. Type 'quit' to exit." << std::endl;
        }
    }
}

int main(int argc, char* argv[])
{
    // 默认连接参数
    std::string serverIp = "127.0.0.1";
    uint16_t serverPort = 8080;
    
    // 解析命令行参数
    if (argc >= 2)
    {
        serverIp = argv[1];
    }
    if (argc >= 3)
    {
        serverPort = static_cast<uint16_t>(std::atoi(argv[2]));
    }

    std::cout << "Chat Client" << std::endl;
    std::cout << "Connecting to " << serverIp << ":" << serverPort << std::endl;
    std::cout << "Type 'quit' or 'exit' to leave" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    muduo::net::EventLoop loop;
    muduo::net::InetAddress serverAddr(serverIp, serverPort);
    ChatClient client(&loop, serverAddr, "ChatClient");

    // 启动输入线程
    std::thread inputThr(inputThread, &client);

    // 连接到服务器
    client.connect();

    // 运行事件循环
    loop.loop();

    // 等待输入线程结束
    inputThr.join();

    std::cout << "Client shutdown." << std::endl;
    return 0;
}