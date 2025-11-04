# Job 服务 Redis 路由功能说明

## 功能概述

Job 服务现已集成 Redis 在线&路由功能，实现了基于 Redis 的连接路由和多 Comet 节点消息分发。

## 架构设计

### 消息分发流程

```
用户发送消息（video_123房间）
    ↓
1. Job 查询 Redis: SMEMBERS room:connections:video_123
   → 获取房间所有连接ID: [conn_1, conn_2, conn_3, ...]
    ↓
2. Job 批量查询: HGETALL connection:info:{conn_id}
   → 获取每个连接的 comet_id
    ↓
3. Job 按 Comet 节点分组
   → localhost:50051: [conn_1, conn_2]
   → localhost:50052: [conn_3, conn_4]
    ↓
4. Job 向每个 Comet 节点发送 gRPC 广播请求
   → BroadcastRoom(room_id, message)
    ↓
5. 房间内所有连接都收到弹幕消息
```

## Redis 数据结构

### 1. 房间连接映射

**Key:** `room:connections:{room_id}`  
**Type:** Set  
**用途:** 存储房间内所有连接ID

```bash
# 示例
SMEMBERS room:connections:video_123
# 返回: ["conn_1", "conn_2", "conn_3"]
```

### 2. 连接信息

**Key:** `connection:info:{conn_id}`  
**Type:** Hash  
**用途:** 存储连接的详细信息

**字段:**
- `comet_id`: Comet 节点地址 (例如: "localhost:50051")
- `user_id`: 用户ID
- `room_id`: 房间ID

```bash
# 示例
HGETALL connection:info:conn_1
# 返回:
# comet_id: localhost:50051
# user_id: user_123
# room_id: video_123
```

### 3. 用户在线状态

**Key:** `user:online:{user_id}`  
**Type:** String  
**用途:** 存储用户在线状态和连接ID

```bash
# 示例
GET user:online:user_123
# 返回: "conn_1"

# 检查用户是否在线
EXISTS user:online:user_123
# 返回: 1 (在线) 或 0 (离线)
```

## 使用示例

### 1. 模拟用户连接

```bash
# 用户 user_123 连接到 Comet (localhost:50051)，连接ID为 conn_1
redis-cli <<EOF
SADD room:connections:video_123 conn_1
HMSET connection:info:conn_1 comet_id localhost:50051 user_id user_123 room_id video_123
SET user:online:user_123 conn_1
EOF
```

### 2. 模拟多用户、多Comet节点

```bash
redis-cli <<EOF
# 用户1: localhost:50051
SADD room:connections:video_123 conn_1
HMSET connection:info:conn_1 comet_id localhost:50051 user_id user_123 room_id video_123
SET user:online:user_123 conn_1

# 用户2: localhost:50051
SADD room:connections:video_123 conn_2
HMSET connection:info:conn_2 comet_id localhost:50051 user_id user_456 room_id video_123
SET user:online:user_456 conn_2

# 用户3: localhost:50052 (另一个Comet节点)
SADD room:connections:video_123 conn_3
HMSET connection:info:conn_3 comet_id localhost:50052 user_id user_789 room_id video_123
SET user:online:user_789 conn_3
EOF
```

### 3. 查询房间连接

```bash
# 查询 video_123 房间有哪些连接
redis-cli SMEMBERS room:connections:video_123
```

### 4. 查询连接信息

```bash
# 查询 conn_1 在哪个 Comet 节点
redis-cli HGET connection:info:conn_1 comet_id
```

### 5. 检查用户在线状态

```bash
# 检查 user_123 是否在线
redis-cli EXISTS user:online:user_123

# 获取用户的连接ID
redis-cli GET user:online:user_123
```

## Job 服务日志输出

启动 Job 服务后，消费 Kafka 消息时会输出以下日志：

```
[Consumer 0] Received PushMsg:
  Type: 1
  Operation: push
  roomId: video_123
  msg: {"content":"Hello World!"}
  
Room video_123 has 3 connections
Grouped 3 connections into 2 comet nodes

[Consumer 0] Broadcasting to Comet localhost:50051 for 2 connections
[Consumer 0] Successfully sent to localhost:50051

[Consumer 0] Broadcasting to Comet localhost:50052 for 1 connections
[Consumer 0] Successfully sent to localhost:50052

[Consumer 0] Message distributed: sent=3, failed=0, comet_nodes=2
```

## 离线补偿机制

### 当前实现

- **WebSocket 路径（Comet直连）**: ✅ 支持
  - 消息存储到 Redis
  - 定时持久化到 MySQL
  - 用户上线时拉取历史消息

- **HTTP → Kafka → Job 路径**: ❌ 不支持
  - 仅实时推送给在线用户
  - 离线用户收不到消息

### 改进建议

在 Job 服务中添加离线消息处理：

```cpp
// 伪代码
for (const auto& conn_id : conn_ids) {
    // 检查用户是否在线
    if (!route_service.IsUserOnline(user_id)) {
        // 存储到离线消息队列
        StoreOfflineMessage(user_id, room_id, message);
        continue;
    }
    
    // 在线用户正常推送
    SendToComet(conn_id, message);
}
```

## 多 Comet 节点部署

### 配置示例

1. 启动多个 Comet 实例：
```bash
# Comet 节点 1
./comet --grpc-port=50051 --ws-port=8082

# Comet 节点 2
./comet --grpc-port=50052 --ws-port=8083
```

2. 用户连接时，Comet 将连接信息写入 Redis：
```cpp
// Comet 服务在 WebSocket 握手成功后
string conn_id = GenerateConnectionId();
string comet_addr = "localhost:50051"; // 当前节点地址

redis->SADD("room:connections:" + room_id, conn_id);
redis->HMSET("connection:info:" + conn_id, 
    "comet_id", comet_addr,
    "user_id", user_id,
    "room_id", room_id);
redis->SET("user:online:" + user_id, conn_id);
```

3. Job 服务自动识别并分发到不同节点

## 性能优化

### 当前实现

- ✅ 批量查询连接信息（减少 Redis 往返）
- ✅ 按 Comet 节点分组（减少 gRPC 调用）
- ✅ 多消费者并发处理（4个线程）

### 可优化点

1. **Redis Pipeline**
   ```cpp
   // 使用 Pipeline 批量查询
   for (auto& conn_id : conn_ids) {
       pipeline.HGETALL("connection:info:" + conn_id);
   }
   auto results = pipeline.execute();
   ```

2. **连接池复用**
   ```cpp
   // 为每个 Comet 节点维护长连接
   // 当前已实现：CometClientManager 缓存 gRPC stub
   ```

3. **本地缓存**
   ```cpp
   // 缓存房间→Comet 映射关系，减少 Redis 查询
   std::map<string, std::map<string, vector<string>>> room_comet_cache;
   ```

## 监控指标

建议添加以下监控指标：

```cpp
// 1. Redis 查询延迟
MetricsCollector::RecordRedisLatency("get_room_connections", latency_ms);

// 2. 消息分发成功率
MetricsCollector::IncrementCounter("job_dispatch_success", comet_addr);
MetricsCollector::IncrementCounter("job_dispatch_failed", comet_addr);

// 3. Comet 节点数量
MetricsCollector::SetGauge("active_comet_nodes", comet_groups.size());

// 4. 房间连接数
MetricsCollector::RecordHistogram("room_connection_count", conn_ids.size());
```

## 故障处理

### Redis 连接失败

- 自动重连机制（每次操作前检查连接）
- 日志记录：`RouteService::CheckConnection()`

### Comet 节点不可达

- gRPC 调用失败时记录日志
- 统计失败数量：`failed_count`
- 建议：实现重试机制或降级策略

### 数据不一致

- 用户断开时，Comet 需要清理 Redis 数据
- 定期清理过期的在线状态（TTL）

```bash
# 设置在线状态过期时间（5分钟）
redis-cli SETEX user:online:user_123 300 conn_1
```

## 测试命令

### 完整测试流程

```bash
# 1. 启动 Redis
redis-server

# 2. 写入测试数据
redis-cli <<EOF
SADD room:connections:video_123 conn_test_1 conn_test_2
HMSET connection:info:conn_test_1 comet_id localhost:50051 user_id user_001 room_id video_123
HMSET connection:info:conn_test_2 comet_id localhost:50051 user_id user_002 room_id video_123
SET user:online:user_001 conn_test_1
SET user:online:user_002 conn_test_2
EOF

# 3. 启动 Comet 服务
cd /home/yang/chatroom/server/build/application/chat-room
./comet

# 4. 启动 Logic 服务（Kafka Producer）
cd /home/yang/chatroom/server/build/application/logic
./logic

# 5. 启动 Job 服务
cd /home/yang/chatroom/server/build/application/job
./job

# 6. 发送测试消息到 Kafka
# (通过 Logic HTTP 接口)

# 7. 查看 Job 日志，确认路由和分发成功
```

## 总结

Job 服务现在支持：

✅ 基于 Redis 的连接路由  
✅ 多 Comet 节点智能分发  
✅ 批量查询优化  
✅ 用户在线状态判断  
✅ 自动重连机制  
