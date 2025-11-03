# 🎉 聊天室监控系统 - 完整实施指南

## ✅ 已完成的工作

### 1. **C++ 监控库集成** ✅

- ✅ 安装 prometheus-cpp 库
- ✅ 创建 `MetricsCollector` 类（封装所有监控指标）
- ✅ 集成到 Comet 服务（chat-room）
- ✅ 添加监控埋点：
  - WebSocket 连接数（构造/析构时自动计数）
  - QPS（每次请求自动计数）
  - 延迟（LatencyTimer RAII 自动计时）
  - 错误率（错误时记录）
  - WebSocket 推送量
  - Redis 操作成功/失败
- ✅ 暴露 `/metrics` 端点（:9091）

### 2. **监控基础设施** ✅

- ✅ Prometheus 配置（`prometheus.yml`）
- ✅ Alertmanager 配置（`alertmanager.yml`）
- ✅ Loki 日志聚合配置（`loki-config.yml`）
- ✅ Promtail 日志采集配置（`promtail-config.yml`）
- ✅ Grafana 数据源和 Dashboard 配置
- ✅ Docker Compose 一键部署

### 3. **告警规则** ✅

创建了 8 大类告警规则（`alert_rules.yml`）：
- 服务可用性（ServiceDown）
- QPS 异常（HighQPS, QPSSurge）
- 延迟告警（HighP99Latency, HighP95Latency）
- 错误率（HighErrorRate, HighErrorCount）
- 连接数异常（HighConnectionCount, ConnectionDrop）
- Kafka 消息积压（KafkaLag）
- WebSocket 推送失败
- gRPC/Redis 操作失败

## 🚀 快速开始

### 步骤 1: 启动监控系统

```bash
cd /home/yang/chatroom/monitoring
./start.sh
```

或手动启动：

```bash
cd /home/yang/chatroom/monitoring
docker compose up -d
```

### 步骤 2: 验证服务状态

```bash
docker compose ps
```

应该看到 5 个服务都在运行：
```
NAME                      STATUS
chatroom-prometheus       Up
chatroom-alertmanager     Up
chatroom-grafana          Up
chatroom-loki             Up
chatroom-promtail         Up
```

### 步骤 3: 启动 Comet 服务

```bash
cd /home/yang/chatroom/server/build
./chat-room
```

### 步骤 4: 验证指标采集

访问 Comet 的 metrics 端点：

```bash
curl http://localhost:9091/metrics
```

应该看到类似输出：

```prometheus
# HELP http_requests_total Total number of HTTP requests
# TYPE http_requests_total counter
http_requests_total{service="comet",endpoint="/ws/clientMessages",method="WS"} 0

# HELP active_connections Number of active connections
# TYPE active_connections gauge
active_connections{service="comet",type="websocket"} 0

# HELP http_request_duration_microseconds HTTP request latency in microseconds
# TYPE http_request_duration_microseconds histogram
http_request_duration_microseconds_bucket{service="comet",endpoint="/ws/clientMessages",le="100"} 0
...
```

### 步骤 5: 访问监控面板

#### Grafana

URL: http://localhost:3000

- 用户名: `admin`
- 密码: `admin123`

首次登录后可以：
1. 左侧菜单 → **Dashboards** → 查看 "聊天室系统监控 - Overview"
2. 左侧菜单 → **Explore** → 选择 Prometheus 数据源 → 输入 PromQL 查询

#### Prometheus

URL: http://localhost:9090

- 查看 Targets: http://localhost:9090/targets
- 查看告警规则: http://localhost:9090/alerts

#### Alertmanager

URL: http://localhost:9093

## 📊 核心监控指标说明

### 1. QPS (每秒请求数)

**指标名称**: `http_requests_total`

**PromQL 查询**:
```promql
# 总体 QPS
rate(http_requests_total[1m])

# 按服务分组
sum(rate(http_requests_total[1m])) by (service)

# 按端点分组
sum(rate(http_requests_total[1m])) by (endpoint)
```

**Grafana 可视化**: Time Series（折线图）

---

### 2. 延迟 (Latency)

**指标名称**: `http_request_duration_microseconds`

**PromQL 查询**:
```promql
# P99 延迟
histogram_quantile(0.99, 
  sum(rate(http_request_duration_microseconds_bucket[5m])) by (le, service)
)

# P95 延迟
histogram_quantile(0.95, 
  sum(rate(http_request_duration_microseconds_bucket[5m])) by (le, service)
)

# 平均延迟
rate(http_request_duration_microseconds_sum[5m]) 
/ 
rate(http_request_duration_microseconds_count[5m])
```

**单位**: 微秒 (μs)
- 1000 μs = 1 ms
- 1000000 μs = 1 s

---

### 3. 活跃连接数

**指标名称**: `active_connections`

**PromQL 查询**:
```promql
active_connections{service="comet"}
```

**Grafana 可视化**: Gauge（仪表盘）或 Time Series

---

### 4. 错误率

**指标名称**: `errors_total`

**PromQL 查询**:
```promql
# 错误率（百分比）
(
  sum(rate(errors_total[5m])) by (service)
  / 
  sum(rate(http_requests_total[5m])) by (service)
) * 100

# 按错误类型统计
sum(rate(errors_total[5m])) by (error_type)
```

---

### 5. Kafka 指标

**指标名称**: `kafka_messages_produced_total`, `kafka_messages_consumed_total`

**PromQL 查询**:
```promql
# 生产速率
rate(kafka_messages_produced_total[1m])

# 消费速率
rate(kafka_messages_consumed_total[1m])

# Lag（积压）
kafka_messages_produced_total - kafka_messages_consumed_total
```

---

### 6. WebSocket 推送

**指标名称**: `websocket_messages_pushed_total`

**PromQL 查询**:
```promql
# 推送速率
rate(websocket_messages_pushed_total[1m])

# 按房间统计
sum(rate(websocket_messages_pushed_total[5m])) by (room_id)
```

---

### 7. gRPC 调用

**指标名称**: `grpc_calls_total`

**PromQL 查询**:
```promql
# 成功率
sum(rate(grpc_calls_total{status="success"}[5m])) by (method)
/ 
sum(rate(grpc_calls_total[5m])) by (method)
```

---

### 8. Redis 操作

**指标名称**: `redis_operations_total`

**PromQL 查询**:
```promql
# 成功率
sum(rate(redis_operations_total{status="success"}[5m])) by (operation)
/ 
sum(rate(redis_operations_total[5m])) by (operation)
```

## 🔔 告警配置

### 配置钉钉告警

1. 编辑 `alertmanager.yml`:

```yaml
webhook_configs:
  - url: 'https://oapi.dingtalk.com/robot/send?access_token=YOUR_TOKEN'
```

2. 获取钉钉 Token:
   - 打开钉钉群
   - 群设置 → 智能群助手 → 添加机器人 → 自定义
   - 复制 Webhook 地址中的 `access_token`

3. 重启 Alertmanager:

```bash
docker compose restart alertmanager
```

### 测试告警

触发一个测试告警（手动停止 Comet 服务）：

```bash
# 停止 Comet
pkill chat-room

# 等待 1 分钟，应该收到 "ServiceDown" 告警

# 重新启动 Comet
cd /home/yang/chatroom/server/build
./chat-room
```

## 📜 日志查询

### 在 Grafana 中查询日志

1. 访问 Grafana: http://localhost:3000
2. 左侧菜单 → **Explore**
3. 选择数据源: **Loki**
4. 输入 LogQL 查询

### 常用 LogQL 查询

```logql
# 查看 Comet 所有日志
{service="comet"}

# 查看错误日志
{service="comet"} |= "ERROR"

# 查看特定关键字
{service="comet"} |= "WebSocket"

# 统计错误数量
sum(count_over_time({service="comet"} |= "ERROR" [5m]))

# 过滤日志级别
{service="comet"} | json | level="ERROR"
```

## 🔧 下一步：集成 Logic 和 Job 服务

目前只集成了 Comet 服务的监控，还需要：

### Logic 服务监控

复制监控代码到 Logic:

```bash
# 1. 复制 monitoring 目录
cp -r /home/yang/chatroom/server/application/chat-room/monitoring \
      /home/yang/chatroom/server/application/logic/

# 2. 修改 Logic 的 CMakeLists.txt（添加 prometheus-cpp 链接）

# 3. 在 Logic 的 main.cc 中初始化监控
MetricsCollector::GetInstance().Initialize("0.0.0.0:9092", "logic");

# 4. 在 Kafka 生产代码中添加监控
MetricsCollector::GetInstance().IncrementKafkaProduced("my-topic");
```

### Job 服务监控

同理集成 Job 服务：

```bash
# 暴露 :9093/metrics
MetricsCollector::GetInstance().Initialize("0.0.0.0:9093", "job");

# 在 Kafka 消费代码中添加监控
MetricsCollector::GetInstance().IncrementKafkaConsumed("my-topic");

# 在 gRPC 调用代码中添加监控
MetricsCollector::GetInstance().IncrementGrpcCall("BroadcastRoom", success);
```

## 🛠️ 故障排查

### 问题 1: Docker 无法启动

```bash
# 检查 Docker 服务状态
sudo systemctl status docker

# 启动 Docker
sudo systemctl start docker
```

### 问题 2: 端口被占用

```bash
# 检查端口占用
sudo netstat -tlnp | grep -E '9090|9091|9092|9093|3000|3100'

# 修改 docker-compose.yml 中的端口映射
```

### 问题 3: Prometheus 无法抓取指标

```bash
# 1. 检查 Comet 是否启动
curl http://localhost:9091/metrics

# 2. 检查 Prometheus Targets
# 访问 http://localhost:9090/targets

# 3. 检查 Docker 网络
docker network inspect monitoring_monitoring
```

### 问题 4: Grafana 无数据

1. 检查数据源配置: **Configuration** → **Data Sources**
2. 测试 Prometheus 连接
3. 检查时间范围（右上角）
4. 检查 PromQL 语法

## 📚 学习资源

- **Prometheus 文档**: https://prometheus.io/docs/
- **Grafana 文档**: https://grafana.com/docs/
- **PromQL 教程**: https://prometheus.io/docs/prometheus/latest/querying/basics/
- **告警规则示例**: https://awesome-prometheus-alerts.grep.to/

## 🎯 完整的监控体系

```
应用层监控:
├── Comet (WebSocket)     ✅ 已完成
├── Logic (HTTP API)      ⏳ 待集成
└── Job (Kafka Worker)    ⏳ 待集成

基础设施监控:
├── Kafka                 💡 推荐: kafka_exporter
├── Redis                 💡 推荐: redis_exporter
├── MySQL                 💡 推荐: mysqld_exporter
└── 系统资源              💡 推荐: node_exporter

日志聚合:
├── Loki                  ✅ 已部署
└── Promtail              ✅ 已配置

告警系统:
├── Alertmanager          ✅ 已部署
├── 钉钉通知              ⏳ 需配置 Token
└── 邮件通知              ⏳ 需配置 SMTP
```

## 🚀 性能优化建议

1. **减少指标基数**: 避免使用高基数标签（如 user_id）
2. **合理设置 scrape_interval**: 默认 15s，可根据需求调整
3. **数据保留期**: 默认 30 天，可在 prometheus.yml 中修改
4. **Loki 日志限制**: 默认每次查询最多 1000 行

## ✅ 总结

恭喜！你已经成功搭建了一套完整的监控系统，包括：

✅ **指标采集**: Prometheus + prometheus-cpp
✅ **可视化**: Grafana Dashboard
✅ **告警**: Alertmanager + 多渠道通知
✅ **日志聚合**: Loki + Promtail
✅ **一键部署**: Docker Compose

现在可以实时监控：
- QPS、延迟、错误率
- 连接数、WebSocket 推送
- Kafka 消息流、gRPC 调用
- Redis 操作、系统日志

享受完整的可观测性体验！🎉
