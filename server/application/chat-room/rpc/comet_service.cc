/*
封装广播逻辑为rpc可调用
*/

#include "comet_service.h"
#include "muduo/base/Logging.h"
#include "websocket_conn.h"
#include "pub_sub_service.h"

// 前向声明 buildWebSocketFrame 函数
extern std::string buildWebSocketFrame(const std::string& payload, uint8_t opcode = 0x01);

extern std::unordered_map<string, CHttpConnPtr> s_user_ws_conn_map;
extern std::mutex s_mtx_user_ws_conn_map_;

namespace ChatRoom {

grpc::Status CometServiceImpl::PushMsg(grpc::ServerContext* context, 
                                     const Comet::PushMsgReq* request,
                                     Comet::PushMsgReply* response) {
    LOG_INFO << "PushMsg called";
    
    // 获取消息内容
    const Protocol::Proto& proto = request->proto();
    
    // 发送给指定的keys
    for (const auto& key : request->keys()) {
        // TODO: 实现具体的消息推送逻辑
    }
    
    return grpc::Status::OK;
}

grpc::Status CometServiceImpl::Broadcast(grpc::ServerContext* context,
                                       const Comet::BroadcastReq* request, 
                                       Comet::BroadcastReply* response) {
    LOG_INFO << "Broadcast called";
    
    // 获取广播消息
    const Protocol::Proto& proto = request->proto();
    int speed = request->speed();
    
    // TODO: 实现广播逻辑
    
    return grpc::Status::OK;
}

grpc::Status CometServiceImpl::BroadcastRoom(grpc::ServerContext* context,
                                           const Comet::BroadcastRoomReq* request,
                                           Comet::BroadcastRoomReply* response) {
   
    
    // 获取房间ID和消息内容
    const std::string& room_id = request->roomid();
    const Protocol::Proto& proto = request->proto();
     LOG_INFO << "BroadcastRoom called, roomID: " << request->roomid() << " proto: " << proto.body();
    // 使用PubSubService广播到指定房间
    auto& pubsub = PubSubService::GetInstance();
    // bool success = pubsub.PublishToRoom(room_id, proto.body());
    bool success = true;
    if (!success) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to broadcast to room");
    }

    std::string ws_frame = buildWebSocketFrame(proto.body());
    auto callback = [&ws_frame, &room_id, this](const std::unordered_set<string> user_ids) {
        LOG_INFO << "room_id:" << room_id << ", callback " <<  ", user_ids.size(): " << user_ids.size();
        for (string userId: user_ids)
        {
             CHttpConnPtr ws_conn_ptr = nullptr;
             {
                std::lock_guard<std::mutex> ulock(s_mtx_user_ws_conn_map_); //自动释放
                ws_conn_ptr =  s_user_ws_conn_map[userId];
             }
             if(ws_conn_ptr) {
                ws_conn_ptr->send(ws_frame);
             } else
             {
                LOG_WARN << "can't find userid: " << userId;
             }
        }
     };

    // 广播给所有人
    PubSubService::GetInstance().PublishMessage(room_id, callback);
    
    return grpc::Status::OK;
}

grpc::Status CometServiceImpl::Rooms(grpc::ServerContext* context,
                                   const Comet::RoomsReq* request,
                                   Comet::RoomsReply* response) {
    LOG_INFO << "Rooms called";
    
    // 获取所有房间列表
    auto& pubsub = PubSubService::GetInstance();
    // const auto& rooms = pubsub.GetAllRooms();
    
    // // 填充响应
    // for (const auto& room : rooms) {
    //     (*response->mutable_rooms())[room.first] = true;
    // }
    
    return grpc::Status::OK;
}

} // namespace ChatRoom 