#include "api_msg.h"
#include "muduo/base/Logging.h"

using namespace std;

int ApiGetRoomHistory(Room &room, MessageBatch &message_batch, const int msg_count) 
{
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    std::string stream_ref =   "+";
    if (!room.history_last_message_id.empty()) {
        stream_ref = "(" + room.history_last_message_id;
    }
    std::vector<std::pair<string, string>> msgs;
    if(cache_conn->GetXrevrange(room.room_id, stream_ref, "-", msg_count, msgs)) {
        //封装
        for(size_t i = 0; i < msgs.size(); i++) {
            // msgs[i].first = "1635724800123-0" (消息ID)
            // msgs[i].second = "{\"content\":\"Hello World\",\"timestamp\":1635724800,\"user_id\":123}"

            Message msg;
            msg.id = msgs[i].first;   //这里保存的是消息id
            room.history_last_message_id = msg.id;  // 保存最后一个消息的id
            Json::Value root;
            Json::Reader jsonReader;
            bool res = jsonReader.parse(msgs[i].second, root);
            if (!res) {
                LOG_ERROR << "parse redis msg failed ";
                return -1;
            }
            if (!root.isObject()) {
                LOG_ERROR << "parsed JSON is not an object"; //增强处理
                return -1;
            }
            if (root["content"].isNull()) {
                LOG_ERROR << "content null";
                return -1;
            }
            msg.content = root["content"].asString();

            if (root["user_id"].isNull()) {
                LOG_ERROR << "content null";
                return -1;
            }
            msg.user_id = root["user_id"].asString();

            if (root["timestamp"].isNull()) {
                LOG_ERROR << "timestamp null";
                return -1;
            }
            msg.timestamp = root["timestamp"].asUInt64();
            message_batch.messages.push_back(msg);
        }
        if(msgs.size() < static_cast<size_t>(msg_count))
            message_batch.has_more = false;     //读取到消息数量 比要求的少了，所以判定为没有更多数据可以读取了
        else
            message_batch.has_more = true;
        return 0;
    } else {
        return -1;
    }
}

// 将 std::vector<Message> 序列化为 JSON 字符串
string SerializeMessagesToJson(const std::vector<Message>& msgs) {
    Json::Value root; // JSON 根节点
    Json::StreamWriterBuilder writer; // JSON 写入器

    // 遍历消息列表
    for (const auto& msg : msgs) {
        Json::Value msgNode; // 单个消息的 JSON 节点
        // 填充消息字段
        msgNode["id"] = msg.id;
        msgNode["content"] = msg.content;
        msgNode["timestamp"] = (Json::UInt64)msg.timestamp;
        msgNode["user_id"] = msg.user_id;

        // 将消息节点添加到根节点
        root.append(msgNode);
    }

    // 将 JSON 树转换为字符串
    return Json::writeString(writer, root);
}

// 将 std::vector<Message> 序列化为 JSON 字符串
string SerializeMessageToJson(const Message msg) {
    Json::Value root; // JSON 根节点
    Json::StreamWriterBuilder writer; // JSON 写入器
    writer.settings_["indentation"] = "";  // 禁用缩进和换行
    // 填充消息字段
    root["content"] = msg.content;
    root["timestamp"] = (Json::UInt64) msg.timestamp;
    root["user_id"] = msg.user_id;

    // 将 JSON 树转换为字符串
    return Json::writeString(writer, root);
}

int ApiStoreMessage(string room_name, std::vector<Message> &msgs)
{
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    for(size_t i = 0; i < msgs.size(); i++) {
        //先将msgs做序列化
        string json_msg = SerializeMessageToJson(msgs[i]);
        std::vector<std::pair<string, string>>  field_value_pairs;
        LOG_INFO << "payload: " << json_msg;
        field_value_pairs.push_back({"payload", json_msg});
        string id = "*";
        bool ret = cache_conn->Xadd(room_name, id, field_value_pairs);
        if(!ret) {
            LOG_ERROR << "ApiStoreMessage room_name: " << room_name << " failed" ;
            return -1;
        }
        LOG_INFO << "msgs id: " << id;
        msgs[i].id = id;
    }
    return 0;
}

// 分级存储：实时存Redis，标记待持久化
int ApiStoreMessageTiered(string room_name, std::vector<Message> &msgs)
{
    // 1. 实时存储到Redis（快速响应用户）
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    if (!cache_conn) {
        LOG_ERROR << "Get Cache connection failed";
        return -1;
    }

    for(size_t i = 0; i < msgs.size(); i++) {
        // 存储到Redis Stream
        string json_msg = SerializeMessageToJson(msgs[i]);
        std::vector<std::pair<string, string>> field_value_pairs;
        field_value_pairs.push_back({"payload", json_msg});
        field_value_pairs.push_back({"room_id", room_name}); // 添加room_id便于批量处理
        string redis_id = "*";
        
        bool redis_ret = cache_conn->Xadd(room_name, redis_id, field_value_pairs);
        if (!redis_ret) {
            LOG_ERROR << "Store message to Redis failed for room: " << room_name;
            return -1;
        }
        
        LOG_INFO << "Message stored to Redis with id: " << redis_id;
        msgs[i].id = redis_id;

        // 2. 添加到待持久化队列（用Redis List作为队列）
        Json::Value persist_msg;
        persist_msg["redis_id"] = redis_id;
        persist_msg["room_id"] = room_name;
        persist_msg["user_id"] = msgs[i].user_id;
        persist_msg["content"] = msgs[i].content;
        persist_msg["timestamp"] = (Json::UInt64)msgs[i].timestamp;
        
        Json::StreamWriterBuilder writer_builder;
        writer_builder.settings_["indentation"] = "";
        string persist_json = Json::writeString(writer_builder, persist_msg);

        // 推入持久化队列
        long queue_len = cache_conn->Lpush("msg_persist_queue", persist_json);
        if (queue_len > 0) {
            LOG_DEBUG << "Added message to persist queue, queue length: " << queue_len;
        }
    }
    
    return 0;
}

// 批量持久化函数（定时调用）
int ApiBatchPersistMessages(int batch_size)
{
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    if (!cache_conn || !db_conn) {
        LOG_ERROR << "Get connection failed";
        return -1;
    }

    // 从队列中批量获取待持久化的消息
    list<string> msg_list;
    bool ret = cache_conn->Lrange("msg_persist_queue", 0, batch_size - 1, msg_list);
    if (!ret || msg_list.empty()) {
        LOG_DEBUG << "No messages to persist";
        return 0;
    }

    LOG_INFO << "Batch persisting " << msg_list.size() << " messages to MySQL";

    // 构建批量插入SQL
    string sql = "INSERT INTO messages (redis_id, room_id, user_id, content, timestamp) VALUES ";
    bool first = true;
    
    for (const auto& msg_json : msg_list) {
        Json::Value msg;
        Json::Reader reader;
        if (!reader.parse(msg_json, msg)) {
            LOG_ERROR << "Parse persist message failed: " << msg_json;
            continue;
        }

        if (!first) sql += ",";
        sql += FormatString("('%s', '%s', '%s', '%s', %lu)",
            msg["redis_id"].asString().c_str(),
            msg["room_id"].asString().c_str(),
            msg["user_id"].asString().c_str(),
            msg["content"].asString().c_str(),
            msg["timestamp"].asUInt64()
        );
        first = false;
    }

    // 执行批量插入
    bool insert_ret = db_conn->ExecuteUpdate(sql.c_str());
    if (insert_ret) {
        // 成功后清空已处理的消息队列（简单方案：删除整个队列）
        // TODO: 后续可以优化为只删除已处理的部分
        cache_conn->Del("msg_persist_queue");
        LOG_INFO << "Successfully persisted " << msg_list.size() << " messages to MySQL";
        return msg_list.size();
    } else {
        LOG_ERROR << "Batch persist to MySQL failed: " << sql;
        return -1;
    }
}

// 分级读取：先从Redis读取近期消息，不足时从MySQL补充
int ApiGetRoomHistoryTiered(Room &room, MessageBatch &message_batch, const int msg_count)
{
    // 1. 先尝试从Redis获取消息（快速访问）
    Room temp_room = room; // 创建副本避免修改原始room
    int redis_result = ApiGetRoomHistory(temp_room, message_batch, msg_count);
    
    // 如果Redis中的消息足够，直接返回
    if (redis_result == 0 && message_batch.messages.size() >= static_cast<size_t>(msg_count)) {
        LOG_INFO << "Got " << message_batch.messages.size() << " messages from Redis";
        return 0;
    }

    // 2. Redis中消息不足，从MySQL补充历史消息
    // LOG_INFO << "Redis messages insufficient, fetching from MySQL";
    
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_slave"); // 使用从库读取
    AUTO_REL_DBCONN(db_manager, db_conn);

    if (!db_conn) {
        LOG_ERROR << "Get DB connection failed";
        return redis_result; // 返回Redis的结果
    }

    // 计算还需要多少条消息
    int needed_count = msg_count - message_batch.messages.size();
    
    // 从MySQL获取更多历史消息
    string sql = FormatString(
        "SELECT id, room_id, user_id, content, timestamp, redis_id FROM messages "
        "WHERE room_id='%s' ORDER BY timestamp DESC LIMIT %d",
        room.room_id.c_str(), needed_count
    );

    CResultSet *result_set = db_conn->ExecuteQuery(sql.c_str());
    if (result_set) {
        while (result_set->Next()) {
            Message msg;
            msg.id = result_set->GetString("redis_id") ? result_set->GetString("redis_id") : 
                     std::to_string(result_set->GetInt("id")); // 优先使用redis_id
            msg.content = result_set->GetString("content");
            msg.user_id = result_set->GetString("user_id");
            msg.timestamp = static_cast<uint64_t>(result_set->GetInt("timestamp"));
            
            message_batch.messages.push_back(msg);
        }
        // LOG_INFO << "Got additional " << message_batch.messages.size() - (redis_result == 0 ? message_batch.messages.size() : 0) 
        //          << " messages from MySQL";
    }

    // 设置是否还有更多消息
    message_batch.has_more = (message_batch.messages.size() >= static_cast<size_t>(msg_count));
    
    return 0;
}