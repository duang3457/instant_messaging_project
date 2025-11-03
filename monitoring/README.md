# 聊天室监控系统部署指南

## 📊 监控架构

```
应用层:
├── Comet (WebSocket)  → :9091/metrics
├── Logic (HTTP API)   → :9092/metrics
└── Job (Kafka)        → :9093/metrics

采集层:
└── Prometheus         → :9090 (时序数据库)

可视化层:
├── Grafana            → :3000 (仪表板)
└── Alertmanager       → :9093 (告警管理)

日志聚合:
├── Loki               → :3100 (日志存储)
└── Promtail           → (日志采集)
```

## 🚀 快速启动

### 1. 启动监控系统

```bash
cd /home/yang/chatroom/monitoring
docker-compose up -d
```

### 2. 检查服务状态

```bash
docker-compose ps
```

应该看到所有服务都是 `Up` 状态：
- chatroom-prometheus
- chatroom-alertmanager
- chatroom-grafana
- chatroom-loki
- chatroom-promtail

### 3. 访问监控面板

- **Grafana**: http://localhost:3000
  - 用户名: `admin`
  - 密码: `admin123`

- **Prometheus**: http://localhost:9090

- **Alertmanager**: http://localhost:9093

### 4. 启动应用服务

启动应用服务后，Prometheus 会自动开始采集指标：

```bash
# 启动 Comet (暴露 :9091/metrics)
cd /home/yang/chatroom/server/build
./chat-room

# 启动 Logic (需要添加监控，暴露 :9092/metrics)
./logic

# 启动 Job (需要添加监控，暴露 :9093/metrics)
./job
```

## 📈 Grafana 仪表板配置

### 导入预定义仪表板

1. 登录 Grafana (http://localhost:3000)
2. 点击左侧菜单 **"+"** → **"Import"**
3. 上传仪表板 JSON 文件或使用以下 Panel 配置

### 核心监控指标

#### 1️⃣ **QPS (每秒请求数)**

**PromQL 查询**:
```promql
# 各服务总体 QPS
sum(rate(http_requests_total[1m])) by (service)

# 按端点分组的 QPS
sum(rate(http_requests_total[1m])) by (service, endpoint)

# WebSocket QPS
rate(http_requests_total{endpoint="/ws/clientMessages"}[1m])
```

**可视化**: Time Series (时间序列图)

---

#### 2️⃣ **延迟 (Latency)**

**PromQL 查询**:
```promql
# P99 延迟（微秒）
histogram_quantile(0.99, 
  sum(rate(http_request_duration_microseconds_bucket[5m])) by (le, service, endpoint)
)

# P95 延迟
histogram_quantile(0.95, 
  sum(rate(http_request_duration_microseconds_bucket[5m])) by (le, service, endpoint)
)

# P50 延迟（中位数）
histogram_quantile(0.50, 
  sum(rate(http_request_duration_microseconds_bucket[5m])) by (le, service, endpoint)
)

# 平均延迟
sum(rate(http_request_duration_microseconds_sum[5m])) by (service, endpoint)
/ 
sum(rate(http_request_duration_microseconds_count[5m])) by (service, endpoint)
```

**可视化**: Time Series + Stat (显示当前值)

---

#### 3️⃣ **活跃连接数**

**PromQL 查询**:
```promql
# 当前 WebSocket 连接数
active_connections{service="comet"}

# 连接数趋势
active_connections[1h]
```

**可视化**: Gauge (仪表盘) + Time Series

---

#### 4️⃣ **错误率**

**PromQL 查询**:
```promql
# 错误率（百分比）
(
  sum(rate(errors_total[5m])) by (service)
  / 
  sum(rate(http_requests_total[5m])) by (service)
) * 100

# 按错误类型统计
sum(rate(errors_total[5m])) by (service, error_type)

# 错误数量
rate(errors_total[1m])
```

**可视化**: Time Series + Bar Gauge

---

#### 5️⃣ **Kafka 指标**

**PromQL 查询**:
```promql
# Kafka 生产速率
rate(kafka_messages_produced_total[1m])

# Kafka 消费速率
rate(kafka_messages_consumed_total[1m])

# Kafka 消息积压（Lag）
kafka_messages_produced_total - kafka_messages_consumed_total
```

**可视化**: Time Series

---

#### 6️⃣ **gRPC 调用**

**PromQL 查询**:
```promql
# gRPC 调用成功率
sum(rate(grpc_calls_total{status="success"}[5m])) by (service, method)
/ 
sum(rate(grpc_calls_total[5m])) by (service, method)

# gRPC 调用 QPS
rate(grpc_calls_total[1m])
```

---

#### 7️⃣ **Redis 操作**

**PromQL 查询**:
```promql
# Redis 操作成功率
sum(rate(redis_operations_total{status="success"}[5m])) by (operation)
/ 
sum(rate(redis_operations_total[5m])) by (operation)

# Redis 操作 QPS
rate(redis_operations_total[1m])
```

---

#### 8️⃣ **WebSocket 推送**

**PromQL 查询**:
```promql
# WebSocket 推送速率
rate(websocket_messages_pushed_total[1m])

# 按房间统计推送量
sum(rate(websocket_messages_pushed_total[5m])) by (room_id)
```

---

## 🔔 告警配置

### 查看告警规则

访问 Prometheus: http://localhost:9090/alerts

### 查看触发的告警

访问 Alertmanager: http://localhost:9093

### 配置钉钉告警

编辑 `alertmanager.yml`，替换钉钉 Webhook Token:

```yaml
webhook_configs:
  - url: 'https://oapi.dingtalk.com/robot/send?access_token=YOUR_DINGTALK_TOKEN'
```

获取钉钉 Token:
1. 打开钉钉群
2. 群设置 → 智能群助手 → 添加机器人 → 自定义
3. 复制 Webhook 地址中的 access_token

## 📜 日志查询

### 在 Grafana 中查询日志

1. 进入 Grafana
2. 左侧菜单 → **Explore**
3. 选择数据源: **Loki**
4. 使用 LogQL 查询:

```logql
# 查询 Comet 服务的所有日志
{service="comet"}

# 查询错误日志
{service="comet"} |= "ERROR"

# 查询特定时间范围的日志
{service="comet"} | json | level="ERROR"

# 统计错误数量
sum(count_over_time({service="comet"} |= "ERROR" [5m]))
```

## 🛠️ 常用操作

### 查看实时指标

访问 Comet 的 metrics 端点:
```bash
curl http://localhost:9091/metrics
```

示例输出:
```
# HELP http_requests_total Total number of HTTP requests
# TYPE http_requests_total counter
http_requests_total{service="comet",endpoint="/ws/clientMessages",method="WS"} 1234

# HELP active_connections Number of active connections
# TYPE active_connections gauge
active_connections{service="comet",type="websocket"} 42

# HELP http_request_duration_microseconds HTTP request latency in microseconds
# TYPE http_request_duration_microseconds histogram
http_request_duration_microseconds_bucket{service="comet",endpoint="/ws/clientMessages",le="100"} 95
http_request_duration_microseconds_bucket{service="comet",endpoint="/ws/clientMessages",le="500"} 145
...
```

### 重启监控系统

```bash
cd /home/yang/chatroom/monitoring
docker-compose restart
```

### 停止监控系统

```bash
docker-compose down
```

### 清理数据（谨慎操作！）

```bash
docker-compose down -v  # 删除所有数据卷
```

## 📊 推荐的 Grafana Dashboard 布局

```
+--------------------------------------------------+
|  聊天室系统监控 - Overview                         |
+--------------------------------------------------+
|  [服务状态]  Comet ✅  Logic ✅  Job ✅           |
+--------------------------------------------------+
|  [QPS]                |  [错误率]                |
|  📈 1234 req/s        |  ⚠️ 0.05%               |
|  ───────────────────  |  ───────────────────   |
|  (时间序列图)          |  (时间序列图)            |
+--------------------------------------------------+
|  [P99 延迟]            |  [活跃连接数]            |
|  ⏱️ 125 ms            |  👥 8,542              |
|  ───────────────────  |  ───────────────────   |
|  (时间序列图)          |  (Gauge)               |
+--------------------------------------------------+
|  [Kafka 消息流]                                   |
|  生产: 500 msg/s  |  消费: 495 msg/s  | Lag: 5  |
|  ───────────────────────────────────────────────|
|  (时间序列图)                                     |
+--------------------------------------------------+
|  [Redis 操作]         |  [gRPC 调用]            |
|  成功率: 99.9%        |  成功率: 100%           |
+--------------------------------------------------+
```

## 🔍 故障排查

### 问题 1: 无法采集指标

**症状**: Prometheus Targets 显示 DOWN

**解决**:
```bash
# 检查服务是否启动
curl http://localhost:9091/metrics

# 检查防火墙
sudo firewall-cmd --list-ports

# 检查 Docker 网络
docker network inspect monitoring_monitoring
```

### 问题 2: Grafana 无数据

**症状**: Dashboard 显示 "No Data"

**解决**:
1. 检查数据源配置: Configuration → Data Sources
2. 测试 Prometheus 连接
3. 检查 PromQL 查询语法

### 问题 3: 日志未采集

**症状**: Loki 中查询不到日志

**解决**:
```bash
# 检查 Promtail 状态
docker logs chatroom-promtail

# 检查日志文件路径
ls -la /home/yang/chatroom/server/build/*.log

# 检查日志文件权限
chmod 644 /home/yang/chatroom/server/build/*.log
```

## 📚 参考文档

- Prometheus: https://prometheus.io/docs/
- Grafana: https://grafana.com/docs/
- Loki: https://grafana.com/docs/loki/
- PromQL: https://prometheus.io/docs/prometheus/latest/querying/basics/
- LogQL: https://grafana.com/docs/loki/latest/logql/

## 🎯 下一步

1. ✅ 集成 Logic 和 Job 服务的监控
2. ✅ 配置钉钉/企业微信告警
3. ✅ 添加系统资源监控（node_exporter）
4. ✅ 添加 Kafka、Redis、MySQL 监控
5. ✅ 创建自定义 Grafana Dashboard
6. ✅ 配置日志告警规则
