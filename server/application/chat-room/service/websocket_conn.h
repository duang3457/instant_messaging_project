
#ifndef __WEBSOCKET_CONN_H__
#define __WEBSOCKET_CONN_H__

#include "http_conn.h"
#include <sstream> // 包含 istringstream 的头文件
#include <openssl/sha.h>
#include "muduo/base/Logging.h" // Logger日志头文件
#include "api_types.h"

class CWebSocketConn: public CHttpConn {
public:
    CWebSocketConn(const muduo::net::TcpConnectionPtr& conn);
    
    virtual void OnRead( muduo::net::Buffer* buf);
    virtual ~CWebSocketConn();
private:
    void sendCloseFrame(uint16_t code, const std::string& reason);
    void sendPongFrame(); // 发送 Pong 帧
    void disconnect();
 // 检查是否是关闭帧
    bool isCloseFrame(const std::string& frame) {
        if (frame.empty()) return false;
        uint8_t opcode = static_cast<uint8_t>(frame[0]) & 0x0F;
        return opcode == 0x8;  // 关闭帧的操作码是 0x8
    }

    int sendHelloMessage();
    int handleClientMessages(Json::Value &root);
    int handleRequestRoomHistory(Json::Value &root);
    
    bool  handshake_completed_ = false;

    string username_;           //用户名
    int32_t userid_ = -1;      //用户id

    std::unordered_map<string, Room> rooms_map_;    //加入的房间
};

using CWebSocketConnPtr = std::shared_ptr<CWebSocketConn>;
// int32_t GetRoomIdsSize();
// std::string GetRoomId(size_t index);
#endif