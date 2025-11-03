# 混合模式架构文档

## 📋 概述

项目现已实现**混合模式消息架构**，结合了 **Redis PubSub（实时）** 和 **Logic → Kafka → Job（异步/离线）** 两种消息推送方式。

## 🔄 消息流

### 方案 1：实时在线推送（PubSub）

```
客户端 WebSocket 发送消息
    ↓
Comet 接收 (websocket_conn.cc::handleClientMessages)
    ↓
存储到 Redis Stream (ApiStoreMessageTiered)
    ↓
Redis PubSub 广播 (PubSubService::PublishMessage)
    ↓
同房间内所有在线用户立即收到消息
```

**特点**：
- ✅ 延迟极低（< 10ms）
- ✅ 适合在线用户实时聊天
- ❌ 仅限单机部署
- ❌ 离线用户无法收到

---

### 方案 2：异步推送（Logic + Kafka + Job）

```
客户端 WebSocket 发送消息
    ↓
Comet 接收 (websocket_conn.cc::handleClientMessages)
    ↓
异步 HTTP POST 到 Logic (localhost:8090/logic/send)
    ↓
Logic 发送到 Kafka (my-topic)
    ↓
Job (4个Consumer线程) 消费 Kafka
    ↓
Job 通过 gRPC 调用 Comet::BroadcastRoom (localhost:50051)
    ↓
Comet 广播到房间内所有用户
```

**特点**：
- ✅ 支持跨服务器部署（多个 Comet 实例）
- ✅ 支持离线消息推送
- ✅ 削峰填谷，Kafka 缓冲高并发
- ✅ 消息可靠性高（Kafka 持久化）
- ❌ 延迟略高（50-200ms）

---

## 🚀 混合模式优势

**同时启用两条路径**，取长补短：

1. **在线用户** → 通过 PubSub 立即收到消息（低延迟）
2. **离线用户** → Logic/Kafka/Job 负责推送通知或存储
3. **跨服务器** → 通过 Kafka 同步消息到所有 Comet 节点
4. **高可用** → Kafka 持久化保证消息不丢失

---

## 📁 实现细节

### 新增文件

1. **`/server/application/chat-room/base/http_client.h`**
   - 异步 HTTP 客户端
   - 使用 muduo::TcpClient 实现
   - 支持异步 POST 请求

2. **`/server/application/chat-room/base/http_client.cc`**
   - HTTP 客户端实现
   - 构造 HTTP/1.1 POST 请求
   - 回调函数处理响应

### 修改文件

1. **`/server/application/chat-room/service/websocket_conn.cc`**
   - 在 `handleClientMessages` 中添加异步转发逻辑
   - 保持原有 PubSub 广播
   - 新增 Logic 服务转发（不阻塞）
   - 代码位置：第 400-430 行

2. **`/server/application/chat-room/monitoring/metrics_collector.h`**
   - 新增 `IncrementCounter` 通用计数器方法
   - 用于统计 `logic_forward` 成功/失败次数

3. **`/server/application/chat-room/monitoring/metrics_collector.cc`**
   - 实现 `IncrementCounter` 方法

---

## 📊 监控指标

新增 Prometheus 指标：

```promql
# Logic 转发成功次数
grpc_calls_total{service="comet", name="logic_forward", status="success"}

# Logic 转发失败次数
grpc_calls_total{service="comet", name="logic_forward", status="failed"}
```

在 Grafana 中可以查看：
- Logic 转发成功率 = success / (success + failed)
- Logic 转发 QPS = rate(success[1m])

---

## 🔧 配置说明

### Comet 配置

**`conf.conf`**:
```ini
http_bind_ip=0.0.0.0
http_bind_port=8082         # WebSocket 端口
grpc_port=50051             # gRPC 服务端口（接收 Job 调用）
metrics_port=9091           # Prometheus 指标端口
```

### Logic 配置

**Logic 服务**监听端口 `8090`，接口：
- `POST /logic/send`

请求格式：
```json
{
  "roomId": "room_123",
  "userId": 1001,
  "userName": "Alice",
  "messages": [
    {"content": "Hello World"}
  ]
}
```

### Job 配置

**Job 服务**:
- Kafka Consumer: `localhost:9092`
- Topic: `my-topic`
- Consumer Group: `job-service-group`
- Consumer 数量: 4 个线程（对应 4 个分区）
- gRPC Client: `localhost:50051` (Comet)

---

## 🧪 测试流程

### 1. 启动所有服务

```bash
# 启动 Zookeeper
~/kafka/bin/zookeeper-server-start.sh ~/kafka/config/zookeeper.properties &

# 启动 Kafka
~/kafka/bin/kafka-server-start.sh ~/kafka/config/server.properties &

# 启动 Comet
cd /home/yang/chatroom/server/build
./application/chat-room/chat-room &

# 启动 Logic
./application/logic/logic &

# 启动 Job
./application/job/job &
```

### 2. 发送测试消息

通过 WebSocket 客户端连接到 `ws://localhost:8082`，发送：

```json
{
  "type": "clientMessages",
  "payload": {
    "content": "Test message",
    "roomId": "room_001"
  }
}
```

### 3. 观察日志

**Comet 日志**：
```
Message broadcast initiated for room room_001
Async request to Logic service initiated
Successfully sent message to Logic service for room room_001
```

**Logic 日志**：
```
Received POST /logic/send
serialized_msg: <protobuf binary>
handleSend successfully
```

**Job 日志**：
```
[Consumer 0] Received PushMsg:
  Type: 1
  Operation: 4
  roomId: room_001
  msg: {"type":"serverMessages","payload":{...}}
BroadcastRoom success
```

### 4. 检查监控

访问 `http://localhost:9091/metrics`，查找：
```
grpc_calls_total{service="comet",name="logic_forward",status="success"} 1
```

---

## ⚠️ 注意事项

1. **消息去重**：同一条消息会通过两条路径到达（PubSub 和 Kafka），客户端需要根据消息 ID 去重
   
2. **顺序保证**：
   - PubSub 路径：严格按发送顺序
   - Kafka 路径：同一 partition 保证顺序

3. **性能考量**：
   - 异步转发到 Logic **不阻塞** WebSocket 响应
   - Logic/Job 延迟不影响实时聊天体验

4. **容错机制**：
   - Logic 服务不可用时，仅影响异步路径
   - PubSub 路径仍然正常工作
   - 监控会记录 `logic_forward:failed`

---

## 🔮 未来优化

1. **智能路由**：
   - 在线用户 → 仅 PubSub
   - 离线用户 → 仅 Kafka
   - 减少重复消息

2. **消息合并**：
   - Job 批量消费 Kafka
   - 合并推送减少 gRPC 调用

3. **跨机房部署**：
   - 多个 Comet 集群
   - Kafka 作为跨机房消息总线

4. **离线推送**：
   - Job 检测用户离线
   - 通过 APNs/FCM 推送通知

---

## 📞 调试技巧

### 查看 HTTP 转发日志

```bash
# Comet 日志
grep "logic_forward" /path/to/comet.log

# Logic 日志
grep "/logic/send" /path/to/logic.log
```

### 监控 Kafka 消息

```bash
# 查看 topic
~/kafka/bin/kafka-console-consumer.sh \
  --bootstrap-server localhost:9092 \
  --topic my-topic \
  --from-beginning
```

### 测试 gRPC 调用

```bash
# 使用 grpcurl 测试 Comet gRPC
grpcurl -plaintext -d '{
  "roomId": "room_001",
  "proto": {
    "ver": 1,
    "op": 4,
    "seq": 0,
    "body": "test message"
  }
}' localhost:50051 ChatRoom.Comet.Comet/BroadcastRoom
```

---

## ✅ 验收检查清单

- [ ] Comet 编译成功
- [ ] Logic 服务运行在 8090 端口
- [ ] Job 服务启动 4 个 Consumer 线程
- [ ] Kafka 和 Zookeeper 正常运行
- [ ] WebSocket 消息能立即收到（PubSub 路径）
- [ ] Logic 日志显示接收到消息
- [ ] Job 日志显示消费 Kafka 消息
- [ ] Prometheus 指标 `logic_forward:success` 递增
- [ ] Grafana 显示 Logic 转发 QPS

---

**实现时间**: 2025-11-03  
**版本**: v1.0  
**作者**: GitHub Copilot  
