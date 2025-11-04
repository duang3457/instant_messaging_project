#include <memory>
#include <thread>
#include <vector>
#include <map>
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
#include "route_service.h"  // Redis路由服务

using namespace muduo;
using namespace muduo::net;

// Comet节点管理器 - 支持多节点路由
class CometClientManager {
public:
    static CometClientManager& getInstance() {
        static CometClientManager instance;
        return instance;
    }

    // 获取指定Comet节点的客户端
    std::shared_ptr<ChatRoom::Comet::Comet::Stub> getClient(const string& comet_addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = clients_.find(comet_addr);
        if (it != clients_.end()) {
            return it->second;
        }

        // 创建新的gRPC连接
        auto channel = grpc::CreateChannel(comet_addr, grpc::InsecureChannelCredentials());
        auto stub = ChatRoom::Comet::Comet::NewStub(channel);
        
        // 将 unique_ptr 转换为 shared_ptr
        std::shared_ptr<ChatRoom::Comet::Comet::Stub> shared_stub = std::move(stub);
        clients_[comet_addr] = shared_stub;
        
        LOG_INFO << "Created new gRPC client for Comet: " << comet_addr;
        return shared_stub;
    }

    // 向指定Comet节点广播消息
    bool broadcastToComet(const string& comet_addr, const string& roomId, const string& msgContent) {
        auto stub = getClient(comet_addr);
        if (!stub) {
            LOG_ERROR << "Failed to get client for Comet: " << comet_addr;
            return false;
        }

        // 创建广播请求
        ChatRoom::Comet::BroadcastRoomReq request;
        request.set_roomid(roomId);
 
        // 创建并设置Proto消息
        ChatRoom::Protocol::Proto *proto = request.mutable_proto();
        proto->set_ver(1);
        proto->set_op(4);
        proto->set_seq(0);
        proto->set_body(msgContent);

        // 发送gRPC请求
        ChatRoom::Comet::BroadcastRoomReply response;
        grpc::ClientContext context;

        grpc::Status status = stub->BroadcastRoom(&context, request, &response);
        if (!status.ok()) {
            LOG_ERROR << "RPC failed to " << comet_addr << ": " << status.error_message();
            return false;
        }
        
        return true;
    }

private:
    std::map<string, std::shared_ptr<ChatRoom::Comet::Comet::Stub>> clients_;
    std::mutex mutex_;
};
 
int main() {
    // 初始化 Redis 路由服务（连接到 Windows 机器上的 Redis）
    RouteService& route_service = RouteService::GetInstance();
    if (!route_service.Init("172.21.176.1", 6379, "")) {
        LOG_ERROR << "Failed to initialize Redis route service";
        return -1;
    }
    LOG_INFO << "Redis route service initialized successfully";

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
                        
                        // ========== 使用 Redis 路由进行消息分发 ==========
                        string room_id = pushMsg.room();
                        string msg_content = pushMsg.msg();
                        
                        // 生成消息唯一ID（用于去重）
                        // 使用 room_id + timestamp + consumer_id 组合
                        string msg_id = room_id + "_" + std::to_string(time(nullptr)) + "_" + std::to_string(i);
                        
                        RouteService& route_service = RouteService::GetInstance();
                        
                        // ========== 步骤1: 消息去重检查（防止重复推送）==========
                        // TTL 60秒：同一条消息在60秒内不会重复处理
                        if (!route_service.CheckAndMarkMessageProcessed(msg_id, room_id, 60)) {
                            LOG_WARN << "[Consumer " << i << "] Duplicate message skipped: " << msg_id;
                            continue; // 跳过重复消息
                        }
                        
                        // ========== 步骤2: 热点房间冷却检查（防止短时间内频繁广播）==========
                        // 冷却时间 1秒：同一房间1秒内只广播一次
                        if (route_service.IsRoomInCooldown(room_id, 1)) {
                            LOG_INFO << "[Consumer " << i << "] Room in cooldown, skipping: " << room_id;
                            continue; // 在冷却期内，跳过
                        }
                        
                        // ========== 步骤3: 分布式锁（防止并发处理同一房间）==========
                        string lock_key = "lock:broadcast:" + room_id;
                        string lock_value = "consumer_" + std::to_string(i) + "_" + std::to_string(time(nullptr));
                        
                        // 尝试获取锁，TTL 5秒（防止死锁）
                        if (!route_service.AcquireLock(lock_key, lock_value, 5)) {
                            LOG_INFO << "[Consumer " << i << "] Another consumer is processing room: " << room_id;
                            continue; // 其他消费者正在处理，跳过
                        }
                        
                        // 获取锁成功，确保最后释放锁
                        // 使用 RAII 自动释放（简化版，实际应使用智能指针）
                        
                        // ========== 步骤4: 查询房间内所有连接 ==========
                        vector<string> conn_ids;
                        if (!route_service.GetRoomConnections(room_id, conn_ids)) {
                            LOG_ERROR << "[Consumer " << i << "] Failed to get room connections for: " << room_id;
                            route_service.ReleaseLock(lock_key, lock_value); // 释放锁
                            continue;
                        }
                        
                        if (conn_ids.empty()) {
                            LOG_INFO << "[Consumer " << i << "] No connections in room: " << room_id;
                            route_service.ReleaseLock(lock_key, lock_value); // 释放锁
                            continue;
                        }
                        
                        // 2. 按Comet节点分组
                        map<string, vector<string>> comet_groups;
                        if (!route_service.GroupConnectionsByComet(conn_ids, comet_groups)) {
                            LOG_ERROR << "[Consumer " << i << "] Failed to group connections by comet";
                            route_service.ReleaseLock(lock_key, lock_value); // 释放锁
                            continue;
                        }
                        
                        // 3. 向每个Comet节点发送广播请求
                        int total_sent = 0;
                        int failed_count = 0;
                        
                        for (const auto& group : comet_groups) {
                            const string& comet_addr = group.first;
                            const vector<string>& conns = group.second;
                            
                            LOG_INFO << "[Consumer " << i << "] Broadcasting to Comet " << comet_addr 
                                     << " for " << conns.size() << " connections";
                            
                            // 发送gRPC广播请求
                            if (CometClientManager::getInstance().broadcastToComet(
                                    comet_addr, room_id, pushMsg.msg())) {
                                total_sent += conns.size();
                                LOG_INFO << "[Consumer " << i << "] Successfully sent to " << comet_addr;
                            } else {
                                failed_count += conns.size();
                                LOG_ERROR << "[Consumer " << i << "] Failed to send to " << comet_addr;
                            }
                        }
                        
                        LOG_INFO << "[Consumer " << i << "] Message distributed: "
                                 << "sent=" << total_sent << ", failed=" << failed_count
                                 << ", comet_nodes=" << comet_groups.size();
                        
                        // ========== 步骤5: 释放分布式锁 ==========
                        route_service.ReleaseLock(lock_key, lock_value);
                        LOG_DEBUG << "[Consumer " << i << "] Lock released for room: " << room_id;
                        
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
