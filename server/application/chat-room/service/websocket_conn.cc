
#include "websocket_conn.h"
#include <openssl/sha.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include "api_types.h"
#include "pub_sub_service.h"
#include "api_msg.h"
#include <jsoncpp/json/json.h>
#include "base64.h"
#include "api_common.h"
using namespace muduo;
using namespace muduo::net;

//key:userid,  value ：websocket的智能指针 基类
std::unordered_map<int32_t, CHttpConnPtr> s_user_ws_conn_map;
std::mutex s_mtx_user_ws_conn_map_;

struct WebSocketFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::string payload_data;
};

std::string extractSid(const std::string& input) {
    // 查找 "sid=" 的位置
    size_t sid_start = input.find("sid=");
    if (sid_start == std::string::npos) {
        return ""; // 如果没有找到 "sid="，返回空字符串
    }

    // 跳过 "sid="，找到值的起始位置
    sid_start += 4; // "sid=" 的长度是 4

    // 查找值的结束位置（假设以空格或字符串结尾为结束）
    size_t sid_end = input.find_first_of(" &", sid_start);
    if (sid_end == std::string::npos) {
        sid_end = input.length();
    }

    // 提取 sid 的值
    std::string sid_value = input.substr(sid_start, sid_end - sid_start);
    return sid_value;
}


std::string generateWebSocketHandshakeResponse(const std::string& key) {
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept_key = key + magic;

    unsigned char sha1[20];
    SHA1(reinterpret_cast<const unsigned char*>(accept_key.data()), accept_key.size(), sha1);

    std::string accept = base64_encode(sha1, 20);

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

    return response;
}

WebSocketFrame parseWebSocketFrame(const std::string& data) {
    WebSocketFrame frame;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());

    // 解析第一个字节
    frame.fin = (bytes[0] & 0x80) != 0;
    frame.opcode = bytes[0] & 0x0F;

    // 解析第二个字节
    frame.mask = (bytes[1] & 0x80) != 0;
    frame.payload_length = bytes[1] & 0x7F;

    size_t offset = 2;

    // 解析扩展长度
    if (frame.payload_length == 126) {
        frame.payload_length = (bytes[2] << 8) | bytes[3];
        offset += 2;
    } else if (frame.payload_length == 127) {
        frame.payload_length = (static_cast<uint64_t>(bytes[2]) << 56) |
                               (static_cast<uint64_t>(bytes[3]) << 48) |
                               (static_cast<uint64_t>(bytes[4]) << 40) |
                               (static_cast<uint64_t>(bytes[5]) << 32) |
                               (static_cast<uint64_t>(bytes[6]) << 24) |
                               (static_cast<uint64_t>(bytes[7]) << 16) |
                               (static_cast<uint64_t>(bytes[8]) << 8)  |
                               static_cast<uint64_t>(bytes[9]);
        offset += 8;
    }

    // 解析掩码
    if (frame.mask) {
        // 如果MASK set to 1, 从data里获取长度为4字节的Masking-key
        std::memcpy(frame.masking_key, bytes + offset, 4);
        offset += 4;
    }

    // 解析有效载荷数据
    frame.payload_data.assign(data.begin() + offset, data.end());

    // 如果掩码存在，解码数据
    if (frame.mask) {
        for (size_t i = 0; i < frame.payload_data.size(); i++) {
            frame.payload_data[i] ^= frame.masking_key[i % 4];
        }
    }
    return frame;
}

CWebSocketConn::CWebSocketConn(const TcpConnectionPtr& conn)
        : CHttpConn(conn)
{
    LOG_INFO << "构造CWebSocketConn";
}

CWebSocketConn::~CWebSocketConn() {
    LOG_INFO << "析构CWebSocketConn";
}

// 构造 WebSocket 数据帧
std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode = 0x01) {
    std::string frame;

    // Fin RSV1 RSV2 RSV3 opcode(4bit)
    //  1    0    0    0  opcode的后四位
    frame.push_back(0x80 | (opcode & 0x0F));

    // MASK  Paylaod len(7bit)最大承载127，
    // 但是126,127是保留值，分别表示
    // 126：接下来用16bit存储长度
    // 127：接下来用64bit存储长度
    size_t payload_length = payload.size();
    if (payload_length <= 125) {
        frame.push_back(static_cast<uint8_t>(payload_length));
    } else if (payload_length <= 65535) {
        frame.push_back(126);
        // >> 8 是取高八位
        // & 0xFF 取低八位
        // 显示将高八位放在前面，低八位放在后面，防止不同架构下顺序不同（防止小端序架构）
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        // 64位同理
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (8 * i)) & 0xFF));
        }
    }
    frame += payload;
    return frame;
}

void CWebSocketConn::OnRead(Buffer* buf)
{
    if (!handshake_completed_) {
        // 处理 WebSocket 握手
        std::string request(buf->retrieveAllAsString());
        LOG_INFO << "request" << request;
        LOG_INFO << "升级为websocket";
        size_t key_start = request.find("Sec-WebSocket-Key: ");

        if (key_start != std::string::npos) {
            key_start += 19;
            size_t key_end = request.find("\r\n", key_start);
            std::string key = request.substr(key_start, key_end - key_start);
            LOG_DEBUG << "key: " << key;

            // 发 response 并自己标志完成
            std::string response = generateWebSocketHandshakeResponse(key);
            send(response);
            handshake_completed_ = true;

            // 校验 cookie   
            string Cookie = headers_["Cookie"];
            string sid;
            if(!Cookie.empty()) {
                sid  = extractSid(Cookie);
                LOG_INFO << "sid = " << sid;
            }
            string email;
            if(Cookie.empty() || ApiGetUserInfoByCookie(username_, userid_, email, sid) != 0) {
                LOG_WARN << "cookie 校验失败" << ", Cookie.empty():" << Cookie.empty() << ", username_:" << username_;
                // 关闭websocket
                // disconnect();
                sendCloseFrame(1008, "Cookie validation failed");
            }else {
                // 校验成功
                // 把连接加入 s_user_ws_conn_map
                LOG_INFO << "cookie validation ok";
                s_mtx_user_ws_conn_map_.lock();
                s_user_ws_conn_map.insert({userid_,shared_from_this()});  // 同样userid连接可能已经存在了
                s_mtx_user_ws_conn_map_.unlock();
                // 订阅房间
                std::vector<Room> &room_list = PubSubService::GetRoomList(); 
                for(size_t i = 0; i < room_list.size(); i++) {
                    rooms_map_.insert({room_list[i].room_id, room_list[i]});
                    PubSubService::GetInstance().AddSubscriber(room_list[i].room_id, userid_);// 订阅对应的聊天室
                }
                // 发送信息给 客户端  
                sendHelloMessage();
            }
        }else {
            LOG_ERROR << "no Sec-WebSocket-Key";    
        }
    } else {
        // 握手完成后的下一次请求
        string request = buf->retrieveAllAsString();
        // 原始帧字节打印出乱码，正常
        // 客户端发来的负载被掩码：需要用 4 字节 mask key 逐字节还原：payload[i] ^= mask[i % 4]
        LOG_INFO << "client -> server: " << request;

        //解析websocket的帧
        WebSocketFrame frame =  parseWebSocketFrame(request);

        LOG_INFO << "prase after: " << frame.payload_data;

        if (frame.opcode == 0x01) { // 文本帧
            //这里写消息处理代码
            bool res;
            Json::Value root;
            Json::Reader jsonReader;
            res = jsonReader.parse(frame.payload_data, root);
            if (!res) {
                LOG_ERROR << "parse login json failed ";
                disconnect();
                return ;
            }
            //获取type字段
            string  type;
            if (root["type"].isNull()) {
                LOG_ERROR << "type null";
                disconnect();
                return;
            }
            type = root["type"].asString();
            if(type == "clientMessages")  {
                handleClientMessages(root);
            } else if(type == "requestRoomHistory") {
                handleRequestRoomHistory(root);
            } else {
                LOG_ERROR << "unknown type: " << type;
            }
        } else if(frame.opcode == 0x08) {  // 0x8 是否为关闭帧
            LOG_INFO<< "Received close frame, closing connection...";
            disconnect();
        } else {
            LOG_ERROR << "can't handle opcode " <<frame.opcode ;
        }
    }
}


// 发送 WebSocket 关闭帧
void CWebSocketConn::sendCloseFrame(uint16_t code, const std::string& reason) {
    if (!tcp_conn_ || !tcp_conn_->connected()) return;
    
    // 构造关闭帧载荷：2字节状态码 + 原因字符串
    std::string payload;
    payload.push_back((code >> 8) & 0xFF);  // 状态码高位
    payload.push_back(code & 0xFF);         // 状态码低位
    payload += reason;                      // 添加原因字符串
    
    // 使用标准WebSocket帧格式构造关闭帧（opcode = 0x08）
    std::string close_frame = buildWebSocketFrame(payload, 0x08);
    tcp_conn_->send(close_frame);
    
    LOG_INFO << "Sent close frame with code: " << code << ", reason: " << reason;
}

// 发送 Pong 帧
void CWebSocketConn::sendPongFrame() {
    if (!tcp_conn_ || !tcp_conn_->connected()) return;

    // 使用标准WebSocket帧格式构造Pong帧（opcode = 0x0A），无载荷
    std::string pong_frame = buildWebSocketFrame("", 0x0A);
    tcp_conn_->send(pong_frame);
    
    LOG_INFO << "Sent Pong frame";
}


void CWebSocketConn::disconnect() {
    try {
        if (tcp_conn_ && tcp_conn_->connected()) {
            // 发送 WebSocket 关闭帧
            sendCloseFrame(1000, "Normal closure");
            tcp_conn_->shutdown();
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Exception in disconnect: " << e.what();
    }
}

// 发送Hello消息给客户端
int CWebSocketConn::sendHelloMessage() {
    try {
        Json::Value root;
        Json::Value payload;
        
        // 构造房间列表
        Json::Value rooms_array(Json::arrayValue);
        for (const auto& room_pair : rooms_map_) {
            Json::Value room_obj;
            room_obj["room_id"] = room_pair.second.room_id;
            room_obj["room_name"] = room_pair.second.room_name;
            room_obj["creator_id"] = room_pair.second.creator_id;
            rooms_array.append(room_obj);
        }
        
        payload["rooms"] = rooms_array;
        payload["username"] = username_;
        payload["user_id"] = userid_;
        
        root["type"] = "hello";
        root["payload"] = payload;
        
        Json::FastWriter writer;
        std::string json_msg = writer.write(root);
        
        // 发送WebSocket帧
        std::string frame = buildWebSocketFrame(json_msg);
        tcp_conn_->send(frame);
        
        LOG_INFO << "Sent hello message to user: " << username_;
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR << "Exception in sendHelloMessage: " << e.what();
        return -1;
    }
}

// 处理客户端消息
int CWebSocketConn::handleClientMessages(Json::Value &root) {
    try {
        std::string type = root["type"].asString();
        LOG_INFO << "Handling client message type: " << type;
        
        if (type == "chat") {
            // 处理聊天消息
            Json::Value payload = root["payload"];
            std::string room_id = payload["room_id"].asString();
            std::string message = payload["message"].asString();
            
            LOG_INFO << "Chat message in room " << room_id << ": " << message;
            
            // 这里可以添加消息处理逻辑，比如保存到数据库
            // 然后通过PubSub广播给房间内的其他用户
            
        } else if (type == "join_room") {
            // 处理加入房间
            Json::Value payload = root["payload"];
            std::string room_id = payload["room_id"].asString();
            
            LOG_INFO << "User " << username_ << " joining room: " << room_id;
            
            // 订阅房间
            PubSubService::GetInstance().AddSubscriber(room_id, userid_);
            
        } else {
            LOG_WARN << "Unknown message type: " << type;
        }
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR << "Exception in handleClientMessages: " << e.what();
        return -1;
    }
}

// 处理房间历史消息请求
int CWebSocketConn::handleRequestRoomHistory(Json::Value &root) {
    try {
        Json::Value payload = root["payload"];
        std::string room_id = payload["room_id"].asString();
        
        LOG_INFO << "Requesting room history for room: " << room_id;
        
        // 构造响应
        Json::Value response;
        Json::Value response_payload;
        
        response_payload["room_id"] = room_id;
        response_payload["messages"] = Json::Value(Json::arrayValue);
        
        response["type"] = "room_history";
        response["payload"] = response_payload;
        
        Json::FastWriter writer;
        std::string json_msg = writer.write(response);
        
        // 发送WebSocket帧
        std::string frame = buildWebSocketFrame(json_msg);
        tcp_conn_->send(frame);
        
        LOG_INFO << "Sent room history for room: " << room_id;
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR << "Exception in handleRequestRoomHistory: " << e.what();
        return -1;
    }
}