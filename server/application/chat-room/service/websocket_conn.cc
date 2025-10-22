
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

//用户id
std::map<int32_t, CHttpConnPtr> s_user_ws_conn_map;

std::mutex mtx_;

// Rooms are static for now.
static std::array<std::string, 5> room_ids{
    "程序员老廖2",
    "码农mark2",
    "程序员yt2",
    "老周讲golang2",
    "绝顶哥编程vico2"
};

// 封装获取房间 ID 的函数
std::string GetRoomId(size_t index) {
    // 检查索引是否越界
    if (index >= room_ids.size()) {
        throw std::out_of_range("Index out of range");
    }
    return room_ids[index];
}

 int32_t GetRoomIdsSize() {
    return room_ids.size();
}

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

struct WebSocketFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::string payload_data;
};

 

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
                               (static_cast<uint64_t>(bytes[8]) << 8) |
                               static_cast<uint64_t>(bytes[9]);
        offset += 8;
    }

    // 解析掩码
    if (frame.mask) {
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

// 构造 WebSocket 数据帧
std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode = 0x01) {
    std::string frame;

    frame.push_back(0x80 | (opcode & 0x0F));

    size_t payload_length = payload.size();
    if (payload_length <= 125) {
        frame.push_back(static_cast<uint8_t>(payload_length));
    } else if (payload_length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (8 * i)) & 0xFF));
        }
    }

    frame += payload;

    return frame;
}
CWebSocketConn::CWebSocketConn(const TcpConnectionPtr& conn)
        : CHttpConn(conn), handshake_completed_(false) 
{
    LOG_INFO << "构造";
}
CWebSocketConn::~CWebSocketConn() {
    LOG_INFO << "析构";
    if(user_id_ != -1) {
        for(int i = 0; i < GetRoomIdsSize(); i++) {
            PubSubService::GetInstance().DeleteSubscriber(GetRoomId(i), user_id_);
        }
        s_user_ws_conn_map.erase(user_id_);
    }
       
}


void CWebSocketConn::OnRead(Buffer* buf)
{
    LOG_INFO << "前端发来消息";

    if (!handshake_completed_) {
        // 处理 WebSocket 握手
        std::string request(buf->retrieveAllAsString());
        url_ = getSubdirectoryFromHttpRequest(request);   // 获取url 后续可以根据不同的url选择不同的处理逻辑，目前没有做抽象，不管url是什么都是同样的处理
        
        if (request.find("Upgrade: websocket") != std::string::npos) {
            LOG_INFO << "升级为websocket";
            size_t key_start = request.find("Sec-WebSocket-Key: ");
            if (key_start != std::string::npos) {
                key_start += 19;
                size_t key_end = request.find("\r\n", key_start);
                std::string key = request.substr(key_start, key_end - key_start);

                std::string response = generateWebSocketHandshakeResponse(key);
                send(response);

                handshake_completed_ = true;
                LOG_INFO << "WebSocket handshake completed";
                // 校验  cookie
                string  Cookie = headers_["Cookie"];
                string  sid;
                if(!Cookie.empty()) {
                     
                    sid  = extractSid(Cookie);
                    LOG_INFO << "sid = " << sid;
                }

                if(Cookie.empty() || GetUsernameByToken(username_, sid) != 0) {
                    LOG_WARN << "cookie 校验失败" << ", Cookie.empty():" << Cookie.empty() << ", username_:" << username_;
                    // 关闭websocket
                    // disconnect();
                     sendCloseFrame(1008, "Cookie validation failed");
                } else 
                {
                      //处理校验正常的逻辑
                    //获取用户 id
                    int ret = GetUserIdByUsername(user_id_, username_);  
                    if(ret != 0) {
                        LOG_WARN << "获取用户id失败";
                        // 关闭websocket
                        disconnect();
                    } else {
                        s_user_ws_conn_map.insert({ user_id_, shared_from_this()});
                        // 发送信息给 客户端  

                        Json::Value root;
                       
                        root["type"] = "hello";
                        Json::Value payload;
                        Json::Value me;
                        me["id"] = user_id_;
                        me["username"] = username_;
                        payload["me"] = me;

                        Json::Value rooms;
                        for(int i = 0; i < GetRoomIdsSize(); i++) {
                            PubSubService::GetInstance().AddSubscriber(GetRoomId(i), user_id_);
                            string  last_message_id;
                            MessageBatch  message_batch;
                            // 获取房间的消息
                            ApiGetRoomHistory(GetRoomId(i), last_message_id, message_batch);     
                            Json::Value  room;  
                            room["id"] = GetRoomId(i);      //聊天室主题名称
                            room["name"] = GetRoomId(i);      //聊天室主题名称 先设置成一样的
                            room["hasMoreMessages"] = message_batch.has_more;
                            Json::Value  messages; 
                            for(int j = 0; j < message_batch.messages.size(); j++) {
                                Json::Value  message;
                                Json::Value user;
                                message["id"] = message_batch.messages[j].id;
                                message["content"] = message_batch.messages[j].content;   
                                user["id"] = user_id_;
                                user["username"] = username_;
                                message["user"] = user;
                                message["timestamp"] = (Json::UInt64)message_batch.messages[j].timestamp;
                                messages[j] = message;
                            }
                            if(message_batch.messages.size() > 0)
                                room["messages"] = messages;
                            else 
                                room["messages"] = Json::arrayValue;  //不能为NULL，否则前端报异常
                            rooms[i] = room;
                        }
                        payload["rooms"] = rooms;
                        root["payload"] = payload;
                        Json::FastWriter writer;
                        string str_json = writer.write(root);
   
                        // 打印 JSON 字符串
                        LOG_INFO << "Serialized JSON: " << str_json;
                        // 封装websocket帧
                        std::string hello = buildWebSocketFrame(str_json);
                        send(hello);
                    } 
                }

            }
        }
    } else {
        // 处理 WebSocket 数据帧
        std::string data = buf->retrieveAllAsString();
        WebSocketFrame frame = parseWebSocketFrame(data);
        
        if (frame.opcode == 0x01) { // 文本帧
            LOG_INFO << "Received WebSocket message: " << frame.payload_data;
            //处理消息， 分析消息的类型进行处理
            //解析type字段，然后根据不同的字段进行处理
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
                //{"type":"clientMessages","payload":{"roomId":"老周讲golang","messages":[{"content":"3"}]}} 
                string roomId;
                Json::Value payload;
                if (root["payload"].isObject()) {
                    payload  = root["payload"];
                }
                roomId = payload["roomId"].asString();
                const Json::Value arrayObj = payload["messages"];
                auto now = timestamp_t::clock::now();
                auto duration = now.time_since_epoch();  // 获取从时钟起点到当前时间点的持续时间
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);  // 转换为秒
                uint64_t timestamp = seconds.count();
                std::vector<Message> msgs;       
                for (int i=0; i<arrayObj.size(); i++)
                {
                    msgs.push_back(Message{
                        "",  // blank ID, will be assigned by Redis
                        std::move(arrayObj[i]["content"].asString()),
                        timestamp,
                        user_id_,
                    });
                }
                // 存储到redis
                int ret = ApiStoreMessage(roomId, msgs);
                //先序列化消息
                Json::Value root;
                // Json::Value payload;
                payload = Json::Value(); //重新置空
                payload["roomId"] = roomId;

                Json::Value  messages; 
                for(int j = 0; j < msgs.size(); j++) {
                    Json::Value  message;
                    Json::Value user;
                    message["id"] = msgs[j].id;
                    message["content"] = msgs[j].content;   
                    user["id"] = user_id_;
                    user["username"] = username_;
                    message["user"] = user;
                    message["timestamp"] = (Json::UInt64)msgs[j].timestamp;
                    messages[j] = message;
                }
                if(msgs.size() > 0)
                    payload["messages"] = messages;
                else 
                    payload["messages"] = Json::arrayValue;  //不能为NULL，否则前端报异常


                root["type"] = "serverMessages";
                root["payload"] = payload;
                Json::FastWriter writer;
                string str_json = writer.write(root);
                //分发
                auto callback = [&str_json, &roomId, &msgs, this](std::unordered_set<int32_t>& userIds) {
                    LOG_INFO << "userIds.size = " << userIds.size();
                    // 遍历所有 userIds 获取对应的连接
                    for (int32_t userId : userIds) {
                        LOG_INFO << "userId: " << userId << ", str_json: " << str_json;
                        CHttpConnPtr ws_conn_ptr =  s_user_ws_conn_map[userId];
                        if(ws_conn_ptr) {
                            //封装成websocket帧
                            // ws_conn_ptr->send(str_json);
                            std::string response = buildWebSocketFrame(str_json);
                            ws_conn_ptr->send(response);
                        } else {
                            LOG_WARN << "userId " << userId << " conn can't find";
                        }
                    }
                };
                LOG_INFO << "广播 消息 to 房间: " << roomId;
                PubSubService::GetInstance().PubSubPublish(roomId, callback );
            } else {
                LOG_ERROR << "can't handle " << type;
            }



            // 回显消息
            // std::string response = buildWebSocketFrame(frame.payload_data);
            // tcp_conn_->send(response);
        }else if(frame.opcode == 0x08) {  // 0x8 是否为关闭帧
            LOG_INFO<< "Received close frame, closing connection...";
            disconnect();
        }  else if(frame.opcode == 0x09) {
            LOG_INFO << "Received Ping frame, sending Pong frame...";
            sendPongFrame();
        }
        else {
            LOG_ERROR << "can't handle opcode " <<frame.opcode ;
        }

    }
}


// 发送 WebSocket 关闭帧
void CWebSocketConn::sendCloseFrame(uint16_t code, const std::string& reason) {
    if (!tcp_conn_) return;

    // 构造关闭帧
    char frame[2 + reason.size()];
    frame[0] = (code >> 8) & 0xFF;  // 状态码高位
    frame[1] = code & 0xFF;         // 状态码低位
    std::memcpy(frame + 2, reason.data(), reason.size());

    // 发送关闭帧
    tcp_conn_->send(frame, sizeof(frame));
}

// 发送 Pong 帧
void CWebSocketConn::sendPongFrame() {
    if (!tcp_conn_) return;

    // 构造 Pong 帧（操作码为 0xA）
    uint8_t frame[2] = {0x8A, 0x00};  // 0x8A 表示 Pong 帧
    tcp_conn_->send(frame, sizeof(frame));
}


void CWebSocketConn::disconnect() {
    if (tcp_conn_) {
        // 发送 WebSocket 关闭帧
        sendCloseFrame(1000, "Normal closure");
        tcp_conn_->shutdown();
    }
}