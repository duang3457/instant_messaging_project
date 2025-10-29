/*
封装广播逻辑为rpc可调用
*/

#pragma once

#include <grpcpp/grpcpp.h>
#include "ChatRoom.Comet.grpc.pb.h"
#include "pub_sub_service.h"

namespace ChatRoom {

class CometServiceImpl final : public Comet::Comet::Service {
public:
    grpc::Status PushMsg(grpc::ServerContext* context, 
                        const Comet::PushMsgReq* request,
                        Comet::PushMsgReply* response) override;

    grpc::Status Broadcast(grpc::ServerContext* context,
                         const Comet::BroadcastReq* request, 
                         Comet::BroadcastReply* response) override;

    grpc::Status BroadcastRoom(grpc::ServerContext* context,
                              const Comet::BroadcastRoomReq* request,
                              Comet::BroadcastRoomReply* response) override;

    grpc::Status Rooms(grpc::ServerContext* context,
                      const Comet::RoomsReq* request,
                      Comet::RoomsReply* response) override;
};

} // namespace ChatRoom 