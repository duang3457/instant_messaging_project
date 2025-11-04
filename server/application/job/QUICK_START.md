# Job 服务 Redis 路由功能 - 快速开始指南

## 📋 功能简介

Job 服务已成功集成 Redis 路由功能，实现：
- ✅ 基于 Redis 的连接管理
- ✅ 多 Comet 节点智能路由
- ✅ 房间消息精准分发
- ✅ 用户在线状态查询

---

## 🚀 快速开始

### 1. 确认 Redis 连接

**当前配置:** Redis 运行在 Windows 机器 `172.21.176.1:6379`

验证连接：
```bash
redis-cli -h 172.21.176.1 -p 6379 ping
# 应返回: PONG
```

### 2. 初始化测试数据

```bash
cd /home/yang/chatroom/server/application/job
./setup_redis_test_data.sh
```

**预期输出:**
```
✅ Redis 连接成功
✅ 用户 user_123 (conn_1) -> localhost:50051
✅ 用户 user_456 (conn_2) -> localhost:50051
✅ 用户 user_789 (conn_3) -> localhost:50052
✅ 用户 user_999 (conn_4) -> localhost:50052

房间 video_123 的连接数: 4
```

### 3. 查看路由数据

```bash
./check_redis_routing.sh video_123
```

**预期输出:**
```
1️⃣  房间连接统计
总连接数: 4

2️⃣  房间内的连接列表
conn_1
conn_2
conn_3
conn_4

3️⃣  按 Comet 节点分组
[localhost:50051] conn_1 -> user_123
[localhost:50051] conn_2 -> user_456
[localhost:50052] conn_3 -> user_789
[localhost:50052] conn_4 -> user_999

5️⃣  在线用户检查
✅ user_123 (连接: conn_1)
✅ user_456 (连接: conn_2)
✅ user_789 (连接: conn_3)
✅ user_999 (连接: conn_4)

在线: 4 | 离线: 0
```

### 4. 启动 Job 服务

```bash
cd /home/yang/chatroom/server/build/application/job
./job
```

**启动日志:**
```
Connected to Redis: 172.21.176.1:6379
Redis route service initialized successfully
Started 4 consumer threads with group: job-service-group
Consumer 0 started successfully
Consumer 1 started successfully
Consumer 2 started successfully
Consumer 3 started successfully
```

---

## 🔄 完整测试流程

### 场景：模拟视频直播弹幕分发

#### 步骤 1: 准备环境

```bash
# 启动 Zookeeper
cd /home/yang/zookeeper-3.4.14/bin
./zkServer.sh start

# 启动 Kafka
cd /home/yang/kafka_2.13-3.9.0
./bin/kafka-server-start.sh config/server.properties &

# 确认 Redis 正在运行（Windows）
```

#### 步骤 2: 初始化 Redis 数据

```bash
cd /home/yang/chatroom/server/application/job
./setup_redis_test_data.sh
```

**模拟场景:**
- 房间 `video_123` 有 4 个在线观众
- 2 个连接到 Comet 节点 1 (`localhost:50051`)
- 2 个连接到 Comet 节点 2 (`localhost:50052`)

#### 步骤 3: 启动服务

**终端 1 - Comet 服务:**
```bash
cd /home/yang/chatroom/server/build/application/chat-room
./comet
```

**终端 2 - Logic 服务:**
```bash
cd /home/yang/chatroom/server/build/application/logic
./logic
```

**终端 3 - Job 服务:**
```bash
cd /home/yang/chatroom/server/build/application/job
./job
```

#### 步骤 4: 发送测试消息

通过 Logic HTTP 接口发送消息：

```bash
curl -X POST http://localhost:8090/logic/send \
  -H "Content-Type: application/json" \
  -d '{
    "roomId": "video_123",
    "userId": 123,
    "userName": "主播",
    "messages": [
      {"content": "大家好，欢迎来到直播间！"}
    ]
  }'
```

#### 步骤 5: 观察 Job 日志

**Job 服务输出:**
```
[Consumer 2] Received PushMsg:
  Type: 1
  Operation: push
  roomId: video_123
  msg: {"content":"大家好，欢迎来到直播间！"}

Room video_123 has 4 connections
Grouped 4 connections into 2 comet nodes

[Consumer 2] Broadcasting to Comet localhost:50051 for 2 connections
[Consumer 2] Successfully sent to localhost:50051

[Consumer 2] Broadcasting to Comet localhost:50052 for 2 connections
[Consumer 2] Successfully sent to localhost:50052

[Consumer 2] Message distributed: sent=4, failed=0, comet_nodes=2
```

**关键观察点:**
1. ✅ Job 从 Redis 查询到 4 个连接
2. ✅ 自动分组为 2 个 Comet 节点
3. ✅ 向每个节点发送 1 次 gRPC 请求（而不是 4 次）
4. ✅ 4 个用户全部收到消息

---

## 📊 Redis 数据结构详解

### 数据存储示意图

```
Redis (172.21.176.1:6379)
│
├─ room:connections:video_123 (Set)
│  ├─ conn_1
│  ├─ conn_2
│  ├─ conn_3
│  └─ conn_4
│
├─ connection:info:conn_1 (Hash)
│  ├─ comet_id: localhost:50051
│  ├─ user_id: user_123
│  └─ room_id: video_123
│
├─ connection:info:conn_2 (Hash)
│  ├─ comet_id: localhost:50051
│  ├─ user_id: user_456
│  └─ room_id: video_123
│
├─ connection:info:conn_3 (Hash)
│  ├─ comet_id: localhost:50052
│  ├─ user_id: user_789
│  └─ room_id: video_123
│
├─ connection:info:conn_4 (Hash)
│  ├─ comet_id: localhost:50052
│  ├─ user_id: user_999
│  └─ room_id: video_123
│
├─ user:online:user_123 (String) -> conn_1
├─ user:online:user_456 (String) -> conn_2
├─ user:online:user_789 (String) -> conn_3
└─ user:online:user_999 (String) -> conn_4
```

### 手动操作 Redis 命令

**查询房间连接:**
```bash
redis-cli -h 172.21.176.1 -p 6379 SMEMBERS room:connections:video_123
```

**查询连接信息:**
```bash
redis-cli -h 172.21.176.1 -p 6379 HGETALL connection:info:conn_1
```

**检查用户在线:**
```bash
redis-cli -h 172.21.176.1 -p 6379 EXISTS user:online:user_123
redis-cli -h 172.21.176.1 -p 6379 GET user:online:user_123
```

**添加新连接:**
```bash
redis-cli -h 172.21.176.1 -p 6379 <<EOF
SADD room:connections:video_123 conn_5
HMSET connection:info:conn_5 comet_id localhost:50051 user_id user_555 room_id video_123
SET user:online:user_555 conn_5
EOF
```

**删除连接:**
```bash
redis-cli -h 172.21.176.1 -p 6379 <<EOF
SREM room:connections:video_123 conn_5
DEL connection:info:conn_5
DEL user:online:user_555
EOF
```

---

## 🛠️ 工具脚本说明

### 1. `setup_redis_test_data.sh`

**功能:** 初始化测试数据

**用法:**
```bash
./setup_redis_test_data.sh
```

**生成数据:**
- 房间 `video_123`
- 4 个用户连接
- 2 个 Comet 节点

### 2. `check_redis_routing.sh`

**功能:** 查看房间路由信息

**用法:**
```bash
# 默认查看 video_123
./check_redis_routing.sh

# 查看指定房间
./check_redis_routing.sh video_456
```

**输出内容:**
- 房间连接统计
- 连接列表
- Comet 节点分组
- 详细连接信息
- 用户在线状态

---

## 🔍 故障排查

### 问题 1: Redis 连接失败

**错误信息:**
```
Failed to initialize Redis route service
Redis connection error: Connection refused
```

**解决方案:**
```bash
# 检查 Redis 是否运行
redis-cli -h 172.21.176.1 -p 6379 ping

# 检查防火墙（Windows）
# 确保端口 6379 允许访问

# 检查 Redis 配置
# redis.conf 中需要设置:
# bind 0.0.0.0
# protected-mode no
```

### 问题 2: 房间没有连接

**错误信息:**
```
Room video_123 has 0 connections
No connections in room: video_123
```

**解决方案:**
```bash
# 重新初始化测试数据
./setup_redis_test_data.sh

# 或手动添加连接
redis-cli -h 172.21.176.1 -p 6379 SADD room:connections:video_123 conn_test
```

### 问题 3: gRPC 调用失败

**错误信息:**
```
RPC failed to localhost:50051: failed to connect to all addresses
Failed to send to localhost:50051
```

**解决方案:**
```bash
# 确保 Comet 服务正在运行
ps aux | grep comet

# 检查 gRPC 端口
netstat -tuln | grep 50051

# 如果 Comet 未启动
cd /home/yang/chatroom/server/build/application/chat-room
./comet
```

### 问题 4: Kafka 消费失败

**错误信息:**
```
Consumer 0 failed to initialize
Local: Broker transport failure
```

**解决方案:**
```bash
# 检查 Kafka 是否运行
ps aux | grep kafka

# 启动 Kafka
cd /home/yang/kafka_2.13-3.9.0
./bin/kafka-server-start.sh config/server.properties &

# 检查 Topic 是否存在
./bin/kafka-topics.sh --list --bootstrap-server localhost:9092
```

---

## 📈 性能监控

### 关键指标

**1. Redis 查询延迟**
```bash
# 使用 redis-cli 监控
redis-cli -h 172.21.176.1 -p 6379 --latency
```

**2. Job 处理速度**
```bash
# 观察日志中的时间戳差异
grep "Message distributed" job.log
```

**3. gRPC 调用成功率**
```bash
# 统计成功和失败的次数
grep "Successfully sent" job.log | wc -l
grep "Failed to send" job.log | wc -l
```

### 预期性能

| 指标 | 目标值 |
|-----|--------|
| Redis 查询延迟 | < 5ms |
| 单条消息分发延迟 | < 50ms |
| 消息处理吞吐量 | > 1000 msg/s |
| gRPC 调用成功率 | > 99% |

---

## 🎯 下一步优化

### 1. 支持配置文件

创建 `conf.conf`:
```ini
redis_host=172.21.176.1
redis_port=6379
redis_password=
```

修改代码读取配置而非硬编码。

### 2. 添加离线消息处理

```cpp
// 检查用户是否在线
if (!route_service.IsUserOnline(user_id)) {
    // 存储到离线队列
    StoreOfflineMessage(user_id, message);
}
```

### 3. 实现连接心跳

定期清理过期的在线状态：
```bash
# 设置 TTL（5分钟）
redis-cli -h 172.21.176.1 -p 6379 SETEX user:online:user_123 300 conn_1
```

### 4. 添加监控埋点

集成 Prometheus 监控指标。

---

## 📚 相关文档

- [REDIS_ROUTING.md](./REDIS_ROUTING.md) - 详细技术文档
- [IMPLEMENTATION_SUMMARY.md](./IMPLEMENTATION_SUMMARY.md) - 实现总结
- [README.md](./README.md) - Job 服务整体说明

---

## ✅ 验收清单

测试完成后，确认以下功能正常：

- [ ] Redis 连接成功（172.21.176.1:6379）
- [ ] 测试数据初始化成功
- [ ] Job 服务启动成功
- [ ] Kafka 消息消费成功
- [ ] Redis 路由查询成功
- [ ] Comet 节点分组正确
- [ ] gRPC 广播调用成功
- [ ] 消息分发统计准确
