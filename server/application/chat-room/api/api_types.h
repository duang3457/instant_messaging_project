#ifndef _API_REGISTER_H_
#define _API_REGISTER_H_
#include "api_common.h"
#include <unordered_map>

using timestamp_t = std::chrono::system_clock::time_point;
 

// 定义 Me 结构体
struct User {
    int id;                        // 用户 ID
    std::string username;          // 用户名
};

// A chat message
struct Message
{
    // Message ID
    std::string id;

    // The actual content of the message
    std::string content;

    // UTC timestamp when the server received the message
    uint64_t timestamp; //直接存储秒的单位

    // ID of the user that sent the message
    std::int64_t user_id{};
};

// A room hiBstory message batch
struct MessageBatch
{
    // The messages in the batch
    std::vector<Message> messages;

    // true if there are more messages that could be loaded
    bool has_more{};
};
 
 
#endif