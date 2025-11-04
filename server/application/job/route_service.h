#ifndef __ROUTE_SERVICE_H__
#define __ROUTE_SERVICE_H__

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <hiredis/hiredis.h>
#include <muduo/base/Logging.h>

using namespace std;

/**
 * Redis 数据结构设计：
 * 
 * 1. room:connections:{room_id} - Set
 *    存储房间内所有连接ID
 *    示例: SMEMBERS room:connections:video_123 -> {conn_1, conn_2, conn_3}
 * 
 * 2. connection:info:{conn_id} - Hash
 *    存储连接信息，包括所属的 comet 节点
 *    示例: HGET connection:info:conn_1 comet_id -> "localhost:50051"
 *          HGET connection:info:conn_1 user_id -> "user_123"
 * 
 * 3. user:online:{user_id} - String
 *    存储用户在线状态和连接ID
 *    示例: GET user:online:user_123 -> "conn_1"
 */

// 连接信息
struct ConnectionInfo {
    string conn_id;      // 连接ID
    string comet_addr;   // Comet节点地址 (例如: "localhost:50051")
    string user_id;      // 用户ID
    string room_id;      // 房间ID
};

// 路由服务 - 负责管理连接和路由信息
class RouteService {
public:
    static RouteService& GetInstance() {
        static RouteService instance;
        return instance;
    }

    RouteService();
    ~RouteService();

    // 初始化Redis连接
    bool Init(const string& redis_host, int redis_port, const string& redis_password = "");

    // 关闭连接
    void Close();

    // ==================== 房间连接管理 ====================
    
    /**
     * 获取房间内所有连接ID
     * @param room_id 房间ID (例如: "video_123")
     * @param conn_ids 返回的连接ID列表
     * @return 成功返回true
     */
    bool GetRoomConnections(const string& room_id, vector<string>& conn_ids);

    // ==================== 连接信息查询 ====================
    
    /**
     * 获取连接的Comet节点地址
     * @param conn_id 连接ID
     * @return Comet地址，失败返回空字符串
     */
    string GetConnectionCometAddr(const string& conn_id);

    /**
     * 批量获取连接信息
     * @param conn_ids 连接ID列表
     * @param conn_infos 返回的连接信息列表
     * @return 成功返回true
     */
    bool GetConnectionInfoBatch(const vector<string>& conn_ids, 
                                 vector<ConnectionInfo>& conn_infos);

    /**
     * 按Comet节点分组连接
     * @param conn_ids 连接ID列表
     * @param comet_groups 返回的分组结果: comet_addr -> [conn_id1, conn_id2, ...]
     * @return 成功返回true
     */
    bool GroupConnectionsByComet(const vector<string>& conn_ids, 
                                  map<string, vector<string>>& comet_groups);

    // ==================== 用户在线状态 ====================
    
    /**
     * 检查用户是否在线
     * @param user_id 用户ID
     * @return 在线返回true
     */
    bool IsUserOnline(const string& user_id);

    /**
     * 获取用户的连接ID
     * @param user_id 用户ID
     * @return 连接ID，离线返回空字符串
     */
    string GetUserConnectionId(const string& user_id);

    // ==================== 分布式锁 ====================
    
    /**
     * 尝试获取分布式锁（基于 Redis SETNX + TTL）
     * @param lock_key 锁的 key
     * @param lock_value 锁的值（通常是唯一标识，如 UUID + 线程ID）
     * @param ttl_seconds 锁的过期时间（秒），防止死锁
     * @return 成功获取返回true
     */
    bool AcquireLock(const string& lock_key, const string& lock_value, int ttl_seconds = 5);

    /**
     * 释放分布式锁（通过 Lua 脚本保证原子性）
     * @param lock_key 锁的 key
     * @param lock_value 锁的值（必须与获取时一致）
     * @return 成功释放返回true
     */
    bool ReleaseLock(const string& lock_key, const string& lock_value);

    // ==================== 消息去重（防止重复推送）====================
    
    /**
     * 检查消息是否已处理（基于 Redis SETNX + TTL）
     * @param msg_id 消息唯一ID（如 Kafka offset 或业务 msg_id）
     * @param room_id 房间ID
     * @param ttl_seconds 去重窗口时间（秒）
     * @return 已处理返回false，未处理返回true并标记为已处理
     */
    bool CheckAndMarkMessageProcessed(const string& msg_id, const string& room_id, int ttl_seconds = 60);

    /**
     * 检查房间是否在热点保护期（防止短时间内重复广播）
     * @param room_id 房间ID
     * @param cooldown_seconds 冷却时间（秒）
     * @return 在冷却期返回true，否则返回false并开始新的冷却期
     */
    bool IsRoomInCooldown(const string& room_id, int cooldown_seconds = 1);

private:
    redisContext* redis_context_;
    string redis_host_;
    int redis_port_;
    string redis_password_;

    // 重新连接Redis
    bool Reconnect();

    // 检查Redis连接状态
    bool CheckConnection();
    
    // 执行 Lua 脚本（用于原子操作）
    bool ExecuteLuaScript(const string& script, const vector<string>& keys, 
                          const vector<string>& args, long long* result = nullptr);
};

#endif // __ROUTE_SERVICE_H__
