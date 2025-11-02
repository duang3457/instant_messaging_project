
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
#include "api_msg.h"
using namespace muduo;
using namespace muduo::net;

//key:userid,  value ：websocket的智能指针 基类
std::unordered_map<string, CHttpConnPtr> s_user_ws_conn_map;
std::mutex s_mtx_user_ws_conn_map_;

struct WebSocketFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::string payload_data;
};

std::string extractUid(const std::string& input) {
    // 查找 "uid=" 的位置
    size_t uid_start = input.find("uid=");
    if (uid_start == std::string::npos) {
        return ""; // 如果没有找到 "uid="，返回空字符串
    }

    // 跳过 "uid="，找到值的起始位置
    uid_start += 4; // "uid=" 的长度是 4

    // 查找值的结束位置（假设以空格或字符串结尾为结束）
    size_t uid_end = input.find_first_of(" &", uid_start);
    if (uid_end == std::string::npos) {
        uid_end = input.length();
    }

    // 提取 uid 的值
    std::string uid_value = input.substr(uid_start, uid_end - uid_start);
    return uid_value;
}

// 从URL中提取uid参数
std::string extractUidFromUrl(const std::string& url) {
    // 查找查询字符串开始位置
    size_t query_start = url.find('?');
    if (query_start == std::string::npos) {
        LOG_INFO << "没有找到？";
        return ""; // 没有查询参数
    }
    
    // 获取查询字符串部分
    std::string query_string = url.substr(query_start + 1);
    
    // 在查询字符串中查找uid参数
    return extractUid(query_string);
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

int CWebSocketConn::handleClientMessages(Json::Value &root) {
    try {
        std::string type = root["type"].asString();
        LOG_INFO << "Handling client message type: " << type;
        
        if (type == "clientMessages") {
            // 处理聊天消息
            Json::Value payload = root["payload"];
            
            // 检查必要字段
            if (payload["content"].isNull() || payload["roomId"].isNull()) {
                LOG_ERROR << "Missing required fields: content or roomId";
                return -1;
            }
            
            std::string room_id = payload["roomId"].asString();
            std::string content = payload["content"].asString();
            uint64_t client_timestamp = payload["timestamp"].isNull() ? 
                static_cast<uint64_t>(time(nullptr) * 1000) : payload["timestamp"].asUInt64();
            
            LOG_INFO << "Chat message from user " << userid_ << " in room " << room_id << ": " << content;
            
            // 创建消息对象
            Message msg;
            msg.content = content;
            msg.user_id = userid_;
            msg.timestamp = static_cast<uint64_t>(time(nullptr)); // 使用服务器时间戳（秒）
            
            // 存储消息到Redis（使用分级存储）
            std::vector<Message> msgs;
            msgs.push_back(msg);
            
            int store_result = ApiStoreMessageTiered(room_id, msgs);
            if (store_result != 0) {
                LOG_ERROR << "Failed to store message for room: " << room_id;
                return -1;
            }
            
            // 获取存储后的消息ID
            msg.id = msgs[0].id;
            LOG_INFO << "Message stored with ID: " << msg.id;
            
            // 通过PubSub广播消息给房间内的其他用户
            Json::Value broadcast_msg;
            Json::Value broadcast_payload;
            
            broadcast_payload["id"] = msg.id;
            broadcast_payload["content"] = msg.content;
            broadcast_payload["timestamp"] = (Json::UInt64)msg.timestamp;
            broadcast_payload["room_id"] = room_id;
            
            // 构造完整的用户对象
            Json::Value user_obj;
            string broadcast_username;
            string broadcast_avatar;
            
            // 查询当前用户信息
            if (ApiGetUserInfoById(userid_, broadcast_username, broadcast_avatar) == 0) {
                user_obj["id"] = userid_;
                user_obj["username"] = broadcast_username;
                user_obj["avatar"] = broadcast_avatar;
            } else {
                // 用户信息查询失败时的默认值
                user_obj["id"] = userid_.empty() ? "0" : userid_;
                user_obj["username"] = "未知用户";
                user_obj["avatar"] = "/img/default.png";
            }
            
            broadcast_payload["user"] = user_obj;
            
            broadcast_msg["type"] = "serverMessages";
            broadcast_msg["payload"] = broadcast_payload;
            
            Json::StreamWriterBuilder writer_builder;
            writer_builder.settings_["indentation"] = "";
            std::string broadcast_json = Json::writeString(writer_builder, broadcast_msg);
            
            // 打印即将广播的消息内容
            LOG_INFO << "准备广播的消息内容: " << broadcast_json;
            
            // 广播给房间内的所有用户
            LOG_INFO << "开始广播消息，房间ID: " << room_id;
            
            PubSubService::GetInstance().PublishMessage(room_id, 
                [broadcast_json, room_id, sender_userid = userid_](const std::unordered_set<string> user_ids) {
                    LOG_INFO << "房间 " << room_id << " 中的订阅用户数量: " << user_ids.size();
                    
                    // 打印所有订阅用户的ID
                    for (const auto& user_id : user_ids) {
                        LOG_INFO << "房间订阅用户ID: " << user_id;
                    }
                    
                    // 检查连接映射表中的用户
                    LOG_INFO << "检查全局连接映射表...";
                    {
                        std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map_);
                        LOG_INFO << "全局连接映射表中的用户数量: " << s_user_ws_conn_map.size();
                        for (const auto& conn_pair : s_user_ws_conn_map) {
                            LOG_INFO << "连接映射表中的用户ID: " << conn_pair.first;
                        }
                    }
                    
                    // 构建WebSocket帧
                    string frame = buildWebSocketFrame(broadcast_json);
                    // 向房间内的所有用户发送消息（除了发送者自己）
                    for (const auto& user_id : user_ids) {
                        // 跳过发送者自己，避免重复发送
                        if (user_id == sender_userid) {
                            LOG_INFO << "Skipping message sender: " << user_id;
                            continue;
                        }
                        
                        std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map_);
                        auto it = s_user_ws_conn_map.find(user_id);
                        if (it != s_user_ws_conn_map.end()) {
                            auto ws_conn = std::dynamic_pointer_cast<CWebSocketConn>(it->second);
                            if (ws_conn && ws_conn->IsConnected()) {
                                ws_conn->SendMessage(frame);
                                LOG_INFO << "Sent message to user: " << user_id;
                            }
                        }
                    }
                });
            
            LOG_INFO << "Message broadcast initiated for room " << room_id;
            
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

// 检查连接是否有效
bool CWebSocketConn::IsConnected() const {
    return tcp_conn_ && tcp_conn_->connected() && handshake_completed_;
}

// 发送消息帧
void CWebSocketConn::SendMessage(const std::string& frame) {
    if (IsConnected()) {
        tcp_conn_->send(frame);
    } else {
        LOG_WARN << "Attempted to send message to disconnected WebSocket";
    }
}

// 处理前端发送的hello消息
int CWebSocketConn::handleHelloMessage(Json::Value &root) {
    (void)root;
    try {
        LOG_INFO << "Handling hello message from client, userid_=" << userid_;
        
        // 从数据库查询用户信息
        string username;
        string avatar;
        if (ApiGetUserInfoById(userid_, username, avatar) != 0) {
            LOG_ERROR << "Failed to get user info for userid: " << userid_;
            username = "未知编程侠";
            avatar = "/img/a.png";
        }
        
        // 构造响应
        Json::Value response;
        Json::Value payload;
        Json::Value user_info;
        
        // 用户信息
        user_info["id"] = userid_;
        user_info["username"] = username;
        user_info["avatar"] = avatar;
        
        // 房间列表和历史消息
        Json::Value rooms_array(Json::arrayValue);
        for (const auto& room_pair : rooms_map_) {
            Json::Value room_obj;
            room_obj["id"] = room_pair.second.room_id;
            room_obj["name"] = room_pair.second.room_name;
            room_obj["users"] = Json::Value(Json::arrayValue); // TODO: 可以后续添加在线用户列表
            
            // 获取房间最新20条历史消息
            Json::Value messages_array(Json::arrayValue);
            try {
                Room temp_room = room_pair.second; // 创建副本
                MessageBatch message_batch;
                
                // 使用分级存储获取历史消息
                int history_result = ApiGetRoomHistoryTiered(temp_room, message_batch, 20);
                
                if (history_result == 0 && !message_batch.messages.empty()) {
                    // LOG_INFO << "Loaded " << message_batch.messages.size() 
                    //          << " history messages for room " << room_pair.second.room_id;
                    
                    // 将消息按时间正序排列（最新消息在后面）
                    std::sort(message_batch.messages.begin(), message_batch.messages.end(),
                        [](const Message& a, const Message& b) {
                            return a.timestamp < b.timestamp;
                        });
                    
                    // 转换为JSON格式
                    for (const auto& msg : message_batch.messages) {
                        Json::Value msg_obj;
                        msg_obj["id"] = msg.id;
                        msg_obj["content"] = msg.content;
                        msg_obj["timestamp"] = (Json::UInt64)msg.timestamp;
                        
                        // 构造完整的用户对象
                        Json::Value user_obj;
                        string msg_username;
                        string msg_avatar;
                        
                        // 查询用户信息
                        if (ApiGetUserInfoById(msg.user_id, msg_username, msg_avatar) == 0) {
                            user_obj["id"] = msg.user_id;
                            user_obj["username"] = msg_username;
                            user_obj["avatar"] = msg_avatar;
                        } else {
                            // 用户信息查询失败时的默认值
                            user_obj["id"] = std::stoi(msg.user_id.empty() ? "0" : msg.user_id);
                            user_obj["username"] = "未知用户";
                            user_obj["avatar"] = "/img/default.png";
                        }
                        
                        msg_obj["user"] = user_obj;
                        messages_array.append(msg_obj);
                    }
                } else {
                    LOG_INFO << "No history messages found for room " << room_pair.second.room_id;
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "Failed to load history for room " << room_pair.second.room_id 
                          << ": " << e.what();
            }
            
            room_obj["messages"] = messages_array;
            rooms_array.append(room_obj);
        }
        
        payload["user"] = user_info;
        payload["rooms"] = rooms_array;
        
        response["type"] = "hello";
        response["payload"] = payload;
        
        Json::FastWriter writer;
        std::string json_msg = writer.write(response);
        
        // 发送WebSocket帧
        std::string frame = buildWebSocketFrame(json_msg);
        tcp_conn_->send(frame);
        
        LOG_INFO << "Sent hello response to user: " << username;
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR << "Exception in handleHelloMessage: " << e.what();
        return -1;
    }
}

void CWebSocketConn::OnRead(Buffer* buf)
{
    if (!handshake_completed_) {
        // 处理 WebSocket 握手
        std::string request(buf->retrieveAllAsString());
        LOG_INFO << "request" << request;
        LOG_INFO << "升级为websocket";
        size_t key_start = request.find("Sec-WebSocket-Key: ");
        // 后端基于Sec-WebSocket-Key: 计算一个Sec-WebSocket-Accept返回给前端

        if (key_start != std::string::npos) {
            key_start += 19;
            size_t key_end = request.find("\r\n", key_start);
            std::string Sec_WebSocket_Key = request.substr(key_start, key_end - key_start);
            LOG_DEBUG << "Sec_WebSocket_Key: " << Sec_WebSocket_Key;

            // 发 response 并标志完成
            std::string response = generateWebSocketHandshakeResponse(Sec_WebSocket_Key);
            send(response);
            handshake_completed_ = true;

            // 以上握手阶段结束
            // 从URL中获取uid参数而不是从Cookie   
            string url = request.substr(0, request.find("\r\n")); // 获取第一行HTTP请求行
            string uid;
            
            // 提取URL中的uid参数
            if (!url.empty()) {
                uid = extractUidFromUrl(url);
                LOG_INFO << "从URL中提取的uid = " << uid;
            }
            
            string email;
            if(uid.empty()) {
                LOG_WARN << "uid参数为空" << ", uid.empty():" << uid.empty() << ", username_:" << username_;
                // 关闭websocket
                sendCloseFrame(1008, "uid validation failed");
            }else {
                // uid不为空，直接设置username为uid
                userid_ = uid;  // 将uid字符串转换为整数后作为userId使用
                // 校验成功
                // 把连接加入 s_user_ws_conn_map
                LOG_INFO << "uid validation ok, username_=" << username_ << ", userid_=" << userid_;
                s_mtx_user_ws_conn_map_.lock();
                s_user_ws_conn_map.insert({userid_, shared_from_this()});  // 同样userid连接可能已经存在了
                s_mtx_user_ws_conn_map_.unlock();
                // 订阅房间
                std::vector<Room> &room_list = PubSubService::GetRoomList(); 
                LOG_INFO << "开始为用户 " << userid_ << " 订阅 " << room_list.size() << " 个房间";
                for(size_t i = 0; i < room_list.size(); i++) {
                    rooms_map_.insert({room_list[i].room_id, room_list[i]});
                    PubSubService::GetInstance().AddSubscriber(room_list[i].room_id, userid_);// 订阅对应的聊天室
                    // LOG_INFO << "用户 " << userid_ << " 已订阅房间: " << room_list[i].room_id << " (" << room_list[i].room_name << ")";
                }
                LOG_INFO << "用户 " << userid_ << " 房间订阅完成";
                // 发送信息给 客户端  
                // sendHelloMessage();
            }
        }else {
            LOG_ERROR << "no Sec-WebSocket-Key";    
        }
    } else {
        // 握手完成后的下一次请求
        string request = buf->retrieveAllAsString();
        // 原始帧字节打印出乱码，正常
        // 客户端发来的负载被掩码：需要用 4 字节 mask key 逐字节还原：payload[i] ^= mask[i % 4]
        // LOG_INFO << "client -> server: " << request;

        //解析websocket的帧
        WebSocketFrame frame =  parseWebSocketFrame(request);

        LOG_INFO << "prase after: " << frame.payload_data;

        // 目前只有文本帧的处理
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
            string type;
            if (root["type"].isNull()) {
                LOG_ERROR << "type null";
                disconnect();
                return;
            }
            type = root["type"].asString();
            // ws的“路由转发”
            // 初次握手之后，前端会发来一个hello，返回初始化数据
            if(type == "hello") {
                // 处理前端发送的hello消息
                handleHelloMessage(root);
            } 
            // 用户每发一条消息，前端会发来一条clientMessages，广播给订阅了房间的所有人
            else if(type == "clientMessages")  {  
                handleClientMessages(root);
            } 
            // 用户向上翻，触发hasMore时，前端发来requestRoomHistory，返回房间历史数据
            else if(type == "requestRoomHistory") {
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