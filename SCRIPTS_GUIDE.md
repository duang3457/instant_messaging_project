# 聊天室服务管理脚本使用指南

## 📁 脚本列表

| 脚本文件 | 功能说明 | 用途 |
|---------|---------|------|
| `start_all.sh` | 启动所有服务 | 按依赖顺序启动 Zookeeper → Kafka → Comet → Logic → Job |
| `stop_all.sh` | 停止所有服务 | 按反向顺序安全停止所有服务 |
| `check_status.sh` | 检查服务状态 | 查看所有服务运行状态和端口监听情况 |

---

## 🚀 快速开始

### 1. 启动所有服务

```bash
cd ~/chatroom
./start_all.sh
```

**预期输出**：
```
========================================
步骤 1/5: 启动 Zookeeper
========================================
[INFO] 启动 Zookeeper...
[INFO] 等待 Zookeeper 启动（端口 2181）...
[SUCCESS] Zookeeper 已启动！

========================================
步骤 2/5: 启动 Kafka
========================================
[INFO] 启动 Kafka...
[INFO] 等待 Kafka 启动（端口 9092）...
[SUCCESS] Kafka 已启动！

... (更多输出)

========================================
所有服务启动完成！
========================================

服务状态：
  ✓ Zookeeper      - 端口 2181
  ✓ Kafka          - 端口 9092
  ✓ Comet          - HTTP/WS: 8082, gRPC: 50051, Metrics: 9091
  ✓ Logic          - 端口 8090
  ✓ Job            - Kafka Consumer (无端口)
```

---

### 2. 检查服务状态

```bash
./check_status.sh
```

**预期输出**：
```
================================================
        聊天室服务状态检查
================================================

【基础服务】

  Zookeeper (2181)           : ✓
  Kafka (9092)               : ✓

【应用服务】

  Comet - WebSocket (8082)   : ✓
  Comet - gRPC (50051)       : ✓
  Comet - Metrics (9091)     : ✓
  Comet - 进程状态           : ✓ (PID: 12345)
  Logic - HTTP (8090)        : ✓
  Logic - 进程状态           : ✓ (PID: 12346)
  Job - 进程状态             : ✓ (PID: 12347)

【监控服务】

  Prometheus (9090)          : ✓
  Grafana (3000)             : ✓

【健康检查】

  Comet Metrics 端点         : ✓
  Prometheus 健康检查        : ✓
  Grafana API 健康检查       : ✓

================================================
【服务总结】

  运行中服务: 5 / 5

  状态: 所有核心服务正常运行 ✓
```

---

### 3. 停止所有服务

```bash
./stop_all.sh
```

**预期输出**：
```
========================================
停止所有聊天室服务
========================================

步骤 1/5: 停止 Job (Kafka 消费者)
[INFO] 停止 Job (PID: 12347)...
[SUCCESS] Job 已停止

步骤 2/5: 停止 Logic (业务逻辑层)
[INFO] 停止 Logic (PID: 12346)...
[SUCCESS] Logic 已停止

... (更多输出)

========================================
验证服务状态
========================================

[SUCCESS] Job 已停止
[SUCCESS] Logic 已停止
[SUCCESS] Comet 已停止
[SUCCESS] Kafka 已停止
[SUCCESS] Zookeeper 已停止

[SUCCESS] 所有服务已成功停止！
```

---

## 📊 服务端口映射

| 服务 | 端口 | 协议 | 说明 |
|------|------|------|------|
| Zookeeper | 2181 | TCP | Kafka 协调服务 |
| Kafka | 9092 | TCP | 消息队列 |
| Comet | 8082 | HTTP/WebSocket | 客户端连接入口 |
| Comet | 50051 | gRPC | 接收 Job 的广播请求 |
| Comet | 9091 | HTTP | Prometheus 监控指标 |
| Logic | 8090 | HTTP | 业务逻辑 API |
| Job | - | - | Kafka Consumer（无监听端口） |
| Prometheus | 9090 | HTTP | 监控服务 |
| Grafana | 3000 | HTTP | 可视化面板 |

---

## 📝 日志文件位置

所有日志文件保存在 `~/chatroom/logs/` 目录：

```bash
~/chatroom/logs/
├── zookeeper.log    # Zookeeper 日志
├── kafka.log        # Kafka 日志
├── comet.log        # Comet 服务日志
├── logic.log        # Logic 服务日志
└── job.log          # Job 服务日志
```

### 查看实时日志

```bash
# 查看 Comet 日志
tail -f ~/chatroom/logs/comet.log

# 查看 Logic 日志
tail -f ~/chatroom/logs/logic.log

# 查看 Job 日志
tail -f ~/chatroom/logs/job.log

# 同时查看多个日志
tail -f ~/chatroom/logs/{comet,logic,job}.log
```

---

## 🔧 常见问题排查

### 问题 1：服务启动失败

**症状**：`start_all.sh` 报错某个服务启动超时

**排查步骤**：

1. 检查日志文件
   ```bash
   tail -100 ~/chatroom/logs/comet.log
   ```

2. 检查端口是否被占用
   ```bash
   netstat -tlnp | grep 8082
   ```

3. 检查配置文件
   ```bash
   cat ~/chatroom/server/build/conf.conf
   ```

---

### 问题 2：Kafka 连接失败

**症状**：Job 日志显示 "Connection refused"

**原因**：Zookeeper 或 Kafka 未启动

**解决方案**：

```bash
# 检查 Zookeeper
netstat -tlnp | grep 2181

# 检查 Kafka
netstat -tlnp | grep 9092

# 如果未启动，单独启动
~/kafka/bin/zookeeper-server-start.sh ~/kafka/config/zookeeper.properties &
~/kafka/bin/kafka-server-start.sh ~/kafka/config/server.properties &
```

---

### 问题 3：Comet 无法连接 Logic

**症状**：Comet 日志显示 "Failed to send message to Logic service"

**排查步骤**：

1. 检查 Logic 服务是否运行
   ```bash
   netstat -tlnp | grep 8090
   ```

2. 手动测试 Logic 接口
   ```bash
   curl -X POST http://localhost:8090/logic/send \
     -H "Content-Type: application/json" \
     -d '{"roomId":"test","userId":1,"userName":"test","messages":[{"content":"hello"}]}'
   ```

---

### 问题 4：服务停止失败

**症状**：`stop_all.sh` 报告服务停止失败

**解决方案**：强制停止

```bash
# 强制停止所有服务
pkill -9 -f 'application/job/job'
pkill -9 -f 'application/logic/logic'
pkill -9 -f 'application/chat-room/chat-room'
pkill -9 -f 'kafka.Kafka'
pkill -9 -f 'QuorumPeerMain'
```

---

## 🎯 高级用法

### 单独启动某个服务

```bash
# 启动 Zookeeper
~/kafka/bin/zookeeper-server-start.sh ~/kafka/config/zookeeper.properties &

# 启动 Kafka
~/kafka/bin/kafka-server-start.sh ~/kafka/config/server.properties &

# 启动 Comet
cd ~/chatroom/server/build
./application/chat-room/chat-room &

# 启动 Logic
./application/logic/logic &

# 启动 Job
./application/job/job &
```

---

### 重启单个服务

```bash
# 重启 Comet
pkill -f 'application/chat-room/chat-room'
sleep 2
cd ~/chatroom/server/build && ./application/chat-room/chat-room > ~/chatroom/logs/comet.log 2>&1 &

# 重启 Logic
pkill -f 'application/logic/logic'
sleep 2
cd ~/chatroom/server/build && ./application/logic/logic > ~/chatroom/logs/logic.log 2>&1 &
```

---

### 监控服务状态（自动刷新）

```bash
# 每 5 秒自动检查一次
watch -n 5 './check_status.sh'
```

---

## 📦 脚本特性

### `start_all.sh` 特性

- ✅ **依赖检查**：启动前检查必要文件是否存在
- ✅ **端口检测**：避免重复启动
- ✅ **智能等待**：等待服务完全就绪后再启动下一个
- ✅ **日志输出**：所有日志保存到文件，便于排查问题
- ✅ **颜色提示**：不同级别的消息使用不同颜色
- ✅ **启动总结**：显示所有服务状态和访问地址

### `stop_all.sh` 特性

- ✅ **优雅停止**：先尝试 SIGTERM，超时后使用 SIGKILL
- ✅ **反向顺序**：按依赖关系反向停止
- ✅ **状态验证**：停止后验证所有服务确实已停止
- ✅ **失败提示**：提供手动强制停止命令

### `check_status.sh` 特性

- ✅ **全面检查**：端口监听、进程状态、健康端点
- ✅ **可视化**：使用 ✓ ✗ 符号清晰展示状态
- ✅ **统计摘要**：显示运行中服务数量
- ✅ **退出码**：可用于自动化脚本（0=全部正常，1=全部停止，2=部分运行）

---

## 🔗 相关文档

- [混合模式架构文档](HYBRID_MODE.md)
- [Prometheus 监控部署指南](monitoring/README.md)
- [完整项目文档](monitoring/DELIVERY.md)

---

## 💡 最佳实践

1. **开发环境**：每次重启机器后运行 `./start_all.sh`
2. **测试环境**：定期运行 `./check_status.sh` 检查服务健康
3. **部署环境**：使用 systemd 或 supervisor 管理服务
4. **调试问题**：先查看 `check_status.sh` 输出，再查看对应日志文件
