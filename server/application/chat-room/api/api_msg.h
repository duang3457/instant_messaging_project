#ifndef _API_MSG_H_
#define _API_MSG_H_
#include "api_common.h"
#include "api_types.h"
const constexpr std::size_t message_batch_size = 50;  //聊天室登录后最多拉取的消息数量,这里并不是翻页的
int ApiGetRoomHistory(string room_name, string &last_message_id, MessageBatch &message_batch);
int ApiStoreMessage(string room_name, std::vector<Message>  &msgs);

//消息序列化
string SerializeMessagesToJson(const std::vector<Message>& msgs);
#endif