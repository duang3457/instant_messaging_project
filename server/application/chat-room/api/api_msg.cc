#include "api_msg.h"
#include <jsoncpp/json/json.h>
#include "muduo/base/Logging.h"

using namespace std;

int ApiGetRoomHistory(string room_name,  string &last_message_id, MessageBatch &message_batch) 
{
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    std::string stream_ref =   "+";
    if (!last_message_id.empty()) {
        stream_ref = stream_ref;
    }
    std::vector<std::pair<string, string>> msgs;
    if(cache_conn->GetXrevrange(room_name, stream_ref, "-", message_batch_size, msgs)) {
        //封装
        for(int i = 0; i < msgs.size(); i++) {
            LOG_INFO << "Key: " << msgs[i].first << ", Value: " << msgs[i].second;

            // msgs[i].first = "1635724800123-0" (消息ID)
            // msgs[i].second = "{\"content\":\"Hello World\",\"timestamp\":1635724800,\"user_id\":123}"

            Message msg;
            msg.id = msgs[i].first;   //这里保存的是消息id

            bool res;
            Json::Value root;
            Json::Reader jsonReader;
            res = jsonReader.parse( msgs[i].second, root);
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
            msg.user_id = root["user_id"].asInt64();

            if (root["timestamp"].isNull()) {
                LOG_ERROR << "timestamp null";
                return -1;
            }
            msg.timestamp = root["timestamp"].asUInt64();
            message_batch.messages.push_back(msg);
        }
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
        msgNode["user_id"] = (Json::Int64)msg.user_id;

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
    root["user_id"] =  (Json::Int64)  msg.user_id;

    // 将 JSON 树转换为字符串
    return Json::writeString(writer, root);
}

int ApiStoreMessage(string room_name, std::vector<Message> &msgs)
{
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    for(int i = 0; i < msgs.size(); i++) {
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