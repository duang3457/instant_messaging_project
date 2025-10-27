#ifndef _API_MSG_H_
#define _API_MSG_H_
#include "api_common.h"
#include "api_types.h"

const constexpr int k_message_batch_size = 30;

int ApiGetRoomHistory(Room &room, MessageBatch &message_batch, const int msg_count = k_message_batch_size);
int ApiStoreMessage(string room_name, std::vector<Message> &msgs);

#endif