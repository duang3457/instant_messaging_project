#include <memory>
#include <thread>
#include <vector>
#include <muduo/net/EventLoop.h>
#include <muduo/base/ThreadPool.h>
#include <muduo/base/Logging.h>

#include <librdkafka/rdkafkacpp.h>
#include <grpcpp/grpcpp.h>
#include "kafka_consumer.h"
#include "ChatRoom.Job.pb.h"
#include "ChatRoom.Comet.pb.h"  
#include "ChatRoom.Comet.grpc.pb.h"  // 使用现有的proto
#include <jsoncpp/json/json.h>

using namespace muduo;
using namespace muduo::net;
 
// gRPC客户端类
class CometClient {
public:
    CometClient(const std::string& server_address) {
        auto channel = grpc::CreateChannel(
            server_address, grpc::InsecureChannelCredentials());
        stub_ = ChatRoom::Comet::Comet::NewStub(channel);
    }

    bool broadcastRoom(const std::string& roomId, const std::string& msgContent) {
        // 创建广播请求
        ChatRoom::Comet::BroadcastRoomReq request;
        request.set_roomid(roomId); //房间ID

        // 创建并设置Proto消息
        ChatRoom::Protocol::Proto *proto = request.mutable_proto();
        proto->set_ver(1);        // 设置版本号
        proto->set_op(4);         // 4代表发送消息操作
        proto->set_seq(0);        // 序列号，可以根据需要设置，用房间作为key, 使用redis 自增
        proto->set_body(msgContent);  // 设置消息内容

        // 发送gRPC请求
        ChatRoom::Comet::BroadcastRoomReply response;
        grpc::ClientContext context;

        grpc::Status status = stub_->BroadcastRoom(&context, request, &response);
        if (!status.ok()) {
            LOG_ERROR << "RPC failed: " << status.error_message();
            return false;
        }
        LOG_INFO << "BroadcastRoom success";
        return true;
    }

private:
    std::unique_ptr<ChatRoom::Comet::Comet::Stub> stub_;
};

// 管理与chatroom6的连接
class CometManager {
public:
    static CometManager& getInstance() {
        static CometManager instance;
        return instance;
    }

    CometClient* getClient() {
        if (!client_) {
            client_ = std::make_unique<CometClient>("localhost:50051");  // chatroom6的地址
        }
        return client_.get();
    }

private:
    std::unique_ptr<CometClient> client_;
};

int main() {
    // 消费者数量（建议与分区数相同，这里设为4）
    const int NUM_CONSUMERS = 4;
    const std::string CONSUMER_GROUP_ID = "job-service-group";
    
    EventLoop loop;
    std::vector<std::thread> consumerThreads;
    
    // 创建多个消费者线程
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumerThreads.emplace_back([i, CONSUMER_GROUP_ID]() {
            // 每个线程创建独立的消费者实例，使用相同的group_id
            KafkaConsumer consumer("localhost:9092", "my-topic", CONSUMER_GROUP_ID);
            if (!consumer.init() || !consumer.subscribe()) {
                LOG_ERROR << "Consumer " << i << " failed to initialize";
                return;
            }
            
            LOG_INFO << "Consumer " << i << " started successfully";
            
            // 持续消费消息
            while (true) {
                std::string message = consumer.consume();
                if (!message.empty()) {
                    ChatRoom::Job::PushMsg pushMsg;
                    if (pushMsg.ParseFromString(message)) {
                        LOG_INFO << "[Consumer " << i << "] Received PushMsg:";
                        LOG_INFO << "  Type: " << static_cast<int>(pushMsg.type());
                        LOG_INFO << "  Operation: " << pushMsg.operation();
                        LOG_INFO << "  roomId: " << pushMsg.room();
                        LOG_INFO << "  msg: " << pushMsg.msg();
                        
                        // 获取chatroom6的客户端并发送消息
                        auto client = CometManager::getInstance().getClient();
                        if (client) {
                            if (!client->broadcastRoom(pushMsg.room(), pushMsg.msg())) {
                                LOG_ERROR << "[Consumer " << i << "] Failed to broadcast message to room: " 
                                        << pushMsg.room();
                            }
                        } else {
                            LOG_ERROR << "[Consumer " << i << "] Failed to get comet client";
                        }
                    } else {
                        LOG_ERROR << "[Consumer " << i << "] Failed to parse message as PushMsg";
                    }
                }
            }
        });
    }
    
    LOG_INFO << "Started " << NUM_CONSUMERS << " consumer threads with group: " << CONSUMER_GROUP_ID;
    
    // 等待所有消费者线程
    for (auto& thread : consumerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return 0;
}
