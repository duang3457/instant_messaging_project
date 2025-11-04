# 聊天室监控埋点位置及指标详细说明

## 📊 监控埋点总览

本文档详细列出了聊天室项目中所有的监控埋点位置、测量指标和用途。

---

## 1️⃣ Comet 服务监控埋点

### 1.1 WebSocket 连接管理

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| WebSocket 连接建立 | `websocket_conn.cc` | 142 | Gauge | `active_connections` | `service=comet` | WebSocket 连接创建时，活跃连接数 +1 |
| WebSocket 连接断开 | `websocket_conn.cc` | 148 | Gauge | `active_connections` | `service=comet` | WebSocket 连接销毁时，活跃连接数 -1 |

**监控目的**：
- 实时跟踪当前活跃的 WebSocket 连接数
- 检测连接泄漏或异常断开
- 告警阈值：`> 10000` 连接

**PromQL 查询**：
```promql
# 当前活跃连接数
active_connections{service="comet"}

# 连接数增长率
rate(active_connections{service="comet"}[5m])
```

---

### 1.2 消息处理 - 客户端消息

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| 请求开始 | `websocket_conn.cc` | 266 | Histogram | `request_duration_microseconds` | `endpoint=/ws/clientMessages`<br>`service=comet` | 使用 RAII LatencyTimer 自动计时 |
| 请求计数 | `websocket_conn.cc` | 269 | Counter | `request_total` | `endpoint=/ws/clientMessages`<br>`method=WS`<br>`service=comet` | 每次处理消息时 +1（用于计算 QPS） |
| 参数缺失错误 | `websocket_conn.cc` | 281 | Counter | `error_total` | `error_type=missing_fields`<br>`endpoint=/ws/clientMessages`<br>`service=comet` | 缺少 content 或 roomId 字段 |
| Redis 存储失败 | `websocket_conn.cc` | 306 | Counter | `error_total` | `error_type=redis_store_failed`<br>`endpoint=/ws/clientMessages`<br>`service=comet` | 消息存储到 Redis 失败 |

**监控目的**：
- 计算消息处理 QPS
- 监控消息处理延迟（P50, P95, P99）
- 检测参数校验错误率
- 监控 Redis 存储成功率

**PromQL 查询**：
```promql
# 消息处理 QPS
rate(request_total{endpoint="/ws/clientMessages"}[1m])

# P99 延迟（微秒）
histogram_quantile(0.99, rate(request_duration_microseconds_bucket{endpoint="/ws/clientMessages"}[5m]))

# 错误率
rate(error_total{endpoint="/ws/clientMessages"}[1m]) / rate(request_total{endpoint="/ws/clientMessages"}[1m])
```

---

### 1.3 Redis 操作监控

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| Redis 存储成功 | `websocket_conn.cc` | 310 | Counter | `redis_ops_total` | `operation=store_message`<br>`status=success`<br>`service=comet` | 消息成功存储到 Redis Stream |

**监控目的**：
- 监控 Redis 操作成功率
- 检测 Redis 连接问题
- 统计 Redis 操作 QPS

**PromQL 查询**：
```promql
# Redis 操作 QPS
rate(redis_ops_total{operation="store_message"}[1m])

# Redis 操作成功率
rate(redis_ops_total{status="success"}[5m]) / rate(redis_ops_total[5m])
```

---

### 1.4 WebSocket 消息推送

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| WebSocket 推送成功 | `websocket_conn.cc` | 402-404 | Counter | `websocket_push_total` | `room_id=xxx`<br>`service=comet` | 每成功推送一条消息到客户端 +1 |

**监控目的**：
- 统计 WebSocket 推送 QPS
- 按房间统计消息分发量
- 检测推送失败情况

**PromQL 查询**：
```promql
# WebSocket 推送 QPS（按房间）
sum by (room_id) (rate(websocket_push_total[1m]))

# 总推送 QPS
sum(rate(websocket_push_total[1m]))
```

---

### 1.5 混合模式 - Logic 服务转发

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| Logic 转发成功 | `websocket_conn.cc` | 437 | Counter | `grpc_calls_total` | `service=comet`<br>`name=logic_forward`<br>`status=success` | HTTP 异步转发到 Logic 成功 |
| Logic 转发失败 | `websocket_conn.cc` | 442 | Counter | `grpc_calls_total` | `service=comet`<br>`name=logic_forward`<br>`status=failed` | HTTP 异步转发到 Logic 失败 |

**监控目的**：
- 监控混合模式下的 Logic 转发成功率
- 检测 Logic 服务可用性
- 统计异步转发 QPS

**PromQL 查询**：
```promql
# Logic 转发成功率
rate(grpc_calls_total{name="logic_forward",status="success"}[5m]) / 
rate(grpc_calls_total{name="logic_forward"}[5m])

# Logic 转发失败率
rate(grpc_calls_total{name="logic_forward",status="failed"}[5m])
```

---

## 2️⃣ Logic 服务监控埋点

### 2.1 HTTP 请求处理

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| POST /logic/send | `logic/main.cc` | - | Counter | `request_total` | `endpoint=/logic/send`<br>`method=POST`<br>`service=logic` | 接收 Comet 转发的消息 |

**监控目的**：
- 统计 Logic 接收消息 QPS
- 监控请求处理延迟
- 检测 HTTP 接口可用性

---

### 2.2 Kafka 消息生产

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| Kafka 生产成功 | `logic/main.cc` | - | Counter | `kafka_produced_total` | `topic=my-topic`<br>`service=logic` | 消息成功发送到 Kafka |
| Kafka 生产失败 | `logic/main.cc` | - | Counter | `error_total` | `error_type=kafka_produce_failed`<br>`service=logic` | Kafka 生产失败 |

**监控目的**：
- 监控 Kafka 生产 QPS
- 检测 Kafka 连接问题
- 统计消息丢失率

**PromQL 查询**：
```promql
# Kafka 生产 QPS
rate(kafka_produced_total{topic="my-topic"}[1m])

# Kafka 生产成功率
rate(kafka_produced_total[5m]) / 
(rate(kafka_produced_total[5m]) + rate(error_total{error_type="kafka_produce_failed"}[5m]))
```

---

## 3️⃣ Job 服务监控埋点

### 3.1 Kafka 消息消费

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| Kafka 消费成功 | `job/main.cc` | - | Counter | `kafka_consumed_total` | `topic=my-topic`<br>`consumer_group=job-service-group`<br>`service=job` | 从 Kafka 成功消费一条消息 |
| Protobuf 解析失败 | `job/main.cc` | - | Counter | `error_total` | `error_type=protobuf_parse_failed`<br>`service=job` | PushMsg 反序列化失败 |

**监控目的**：
- 统计 Kafka 消费 QPS
- 监控 Consumer Lag（待消费消息数）
- 检测消息格式错误率

**PromQL 查询**：
```promql
# Kafka 消费 QPS
rate(kafka_consumed_total{topic="my-topic"}[1m])

# Consumer Lag（需要 JMX Exporter）
kafka_consumer_group_lag{group="job-service-group"}
```

---

### 3.2 gRPC 调用 Comet

| 埋点位置 | 文件路径 | 代码行 | 指标类型 | 指标名称 | 标签 | 说明 |
|---------|---------|--------|---------|---------|------|------|
| gRPC 调用成功 | `job/main.cc` | - | Counter | `grpc_calls_total` | `method=BroadcastRoom`<br>`status=success`<br>`service=job` | 成功调用 Comet.BroadcastRoom |
| gRPC 调用失败 | `job/main.cc` | - | Counter | `grpc_calls_total` | `method=BroadcastRoom`<br>`status=failed`<br>`service=job` | gRPC 调用失败 |

**监控目的**：
- 监控 gRPC 调用成功率
- 检测 Comet 服务可用性
- 统计 gRPC 调用延迟

**PromQL 查询**：
```promql
# gRPC 调用成功率
rate(grpc_calls_total{method="BroadcastRoom",status="success"}[5m]) / 
rate(grpc_calls_total{method="BroadcastRoom"}[5m])

# gRPC 调用失败率
rate(grpc_calls_total{method="BroadcastRoom",status="failed"}[5m])
```

---

## 4️⃣ 监控指标汇总表

### 4.1 核心业务指标

| 指标名称 | 类型 | 用途 | 告警阈值 | 计算方式 |
|---------|------|------|---------|---------|
| **QPS（每秒请求数）** | Counter | 系统吞吐量 | - | `rate(request_total[1m])` |
| **P99 延迟** | Histogram | 99% 请求响应时间 | `> 1s` | `histogram_quantile(0.99, rate(request_duration_microseconds_bucket[5m]))` |
| **错误率** | Counter | 请求失败比例 | `> 5%` | `rate(error_total[1m]) / rate(request_total[1m])` |
| **活跃连接数** | Gauge | 当前在线用户 | `> 10000` | `active_connections` |
| **WebSocket 推送成功率** | Counter | 消息推送成功比例 | `< 95%` | `rate(websocket_push_total[5m])` |

### 4.2 中间件指标

| 指标名称 | 类型 | 用途 | 告警阈值 | 计算方式 |
|---------|------|------|---------|---------|
| **Redis 操作成功率** | Counter | Redis 健康状态 | `< 99%` | `rate(redis_ops_total{status="success"}[5m]) / rate(redis_ops_total[5m])` |
| **Kafka 生产 QPS** | Counter | Kafka 写入速率 | - | `rate(kafka_produced_total[1m])` |
| **Kafka 消费 QPS** | Counter | Kafka 消费速率 | - | `rate(kafka_consumed_total[1m])` |
| **Kafka Consumer Lag** | Gauge | 待消费消息积压 | `> 1000` | `kafka_consumer_group_lag` |
| **gRPC 调用成功率** | Counter | 服务间调用健康 | `< 95%` | `rate(grpc_calls_total{status="success"}[5m]) / rate(grpc_calls_total[5m])` |

### 4.3 系统资源指标

| 指标名称 | 类型 | 用途 | 告警阈值 | 数据源 |
|---------|------|------|---------|--------|
| **CPU 使用率** | Gauge | 系统负载 | `> 80%` | Node Exporter |
| **内存使用率** | Gauge | 内存压力 | `> 85%` | Node Exporter |
| **磁盘使用率** | Gauge | 存储空间 | `> 90%` | Node Exporter |
| **网络流量** | Counter | 带宽占用 | - | Node Exporter |

---

## 5️⃣ Grafana 仪表盘面板配置

### 5.1 核心业务面板

| 面板名称 | 查询语句 | 可视化类型 | 说明 |
|---------|---------|-----------|------|
| **总 QPS** | `sum(rate(request_total[1m]))` | Graph | 显示所有服务的总请求速率 |
| **各服务 QPS** | `sum by (service) (rate(request_total[1m]))` | Graph | 按服务分组显示 QPS |
| **P99 延迟** | `histogram_quantile(0.99, rate(request_duration_microseconds_bucket[5m]))` | Graph | 99% 请求的响应时间 |
| **错误率** | `rate(error_total[1m]) / rate(request_total[1m]) * 100` | Graph | 错误请求百分比 |
| **活跃连接数** | `active_connections{service="comet"}` | Stat | 当前 WebSocket 连接数 |

### 5.2 消息流转面板

| 面板名称 | 查询语句 | 可视化类型 | 说明 |
|---------|---------|-----------|------|
| **WebSocket 推送 QPS** | `sum(rate(websocket_push_total[1m]))` | Graph | WebSocket 消息推送速率 |
| **按房间推送分布** | `topk(10, sum by (room_id) (rate(websocket_push_total[1m])))` | Bar Chart | Top 10 活跃房间 |
| **Kafka 生产 vs 消费** | `rate(kafka_produced_total[1m])` vs `rate(kafka_consumed_total[1m])` | Graph | Kafka 吞吐量对比 |
| **Logic 转发成功率** | `rate(grpc_calls_total{name="logic_forward",status="success"}[5m]) / rate(grpc_calls_total{name="logic_forward"}[5m]) * 100` | Gauge | 混合模式转发健康度 |

### 5.3 中间件面板

| 面板名称 | 查询语句 | 可视化类型 | 说明 |
|---------|---------|-----------|------|
| **Redis 操作 QPS** | `sum(rate(redis_ops_total[1m]))` | Graph | Redis 操作速率 |
| **Redis 成功率** | `rate(redis_ops_total{status="success"}[5m]) / rate(redis_ops_total[5m]) * 100` | Gauge | Redis 健康度 |
| **Kafka Consumer Lag** | `kafka_consumer_group_lag{group="job-service-group"}` | Graph | Kafka 消息积压 |
| **gRPC 调用延迟** | `histogram_quantile(0.99, rate(grpc_duration_seconds_bucket[5m]))` | Graph | gRPC 调用 P99 延迟 |

---

## 6️⃣ 告警规则配置

### 6.1 服务可用性告警

```yaml
groups:
  - name: service_health
    rules:
      - alert: ServiceDown
        expr: up{job=~"comet|logic|job"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "服务 {{ $labels.job }} 已停止"
          description: "{{ $labels.instance }} 无法访问"

      - alert: HighErrorRate
        expr: rate(error_total[5m]) / rate(request_total[5m]) > 0.05
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "{{ $labels.service }} 错误率过高"
          description: "错误率: {{ $value | humanizePercentage }}"
```

### 6.2 性能告警

```yaml
  - name: performance
    rules:
      - alert: HighLatency
        expr: histogram_quantile(0.99, rate(request_duration_microseconds_bucket[5m])) > 1000000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "{{ $labels.endpoint }} P99 延迟过高"
          description: "P99 延迟: {{ $value | humanizeDuration }}"

      - alert: HighConnectionCount
        expr: active_connections > 10000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "WebSocket 连接数过高"
          description: "当前连接: {{ $value }}"
```

### 6.3 中间件告警

```yaml
  - name: middleware
    rules:
      - alert: KafkaConsumerLag
        expr: kafka_consumer_group_lag > 1000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Kafka Consumer Lag 过高"
          description: "Consumer Group: {{ $labels.group }}, Lag: {{ $value }}"

      - alert: RedisOperationFailed
        expr: rate(redis_ops_total{status="success"}[5m]) / rate(redis_ops_total[5m]) < 0.95
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Redis 操作成功率过低"
          description: "成功率: {{ $value | humanizePercentage }}"
```

---

## 7️⃣ 监控最佳实践

### 7.1 日志与监控关联

- 在监控告警触发时，使用 **Loki** 查询对应时间段的日志
- 在 Grafana 面板中添加日志链接，一键跳转到相关日志

**示例**：
```promql
# 错误日志查询（Loki）
{service="comet"} |= "ERROR" | json | error_type="redis_store_failed"
```

### 7.2 监控埋点规范

1. **命名规范**：
   - Counter: `xxx_total`
   - Gauge: `xxx_current` 或直接名称
   - Histogram: `xxx_duration_microseconds`

2. **标签规范**：
   - 必须包含 `service` 标签
   - 错误类型使用 `error_type` 标签
   - 端点使用 `endpoint` 标签

3. **埋点原则**：
   - 关键路径必须埋点
   - 外部依赖调用必须埋点
   - 错误处理必须埋点

### 7.3 性能优化建议

- **避免高基数标签**：不要使用用户 ID、消息 ID 作为标签
- **使用 LatencyTimer**：自动计时，避免手动计算
- **异步上报**：监控数据上报不应阻塞业务逻辑

---

## 8️⃣ 监控数据导出

### 8.1 Prometheus Metrics 端点

| 服务 | Metrics URL | 说明 |
|------|-------------|------|
| Comet | `http://localhost:9091/metrics` | Comet 服务监控指标 |
| Logic | `http://localhost:9092/metrics` | Logic 服务监控指标（待实现） |
| Job | `http://localhost:9093/metrics` | Job 服务监控指标（待实现） |

### 8.2 查看原始指标

```bash
# 查看 Comet 的所有指标
curl http://localhost:9091/metrics

# 过滤特定指标
curl http://localhost:9091/metrics | grep request_total

# 实时监控指标变化
watch -n 1 'curl -s http://localhost:9091/metrics | grep active_connections'
```

---

## 📚 相关文档

- [监控系统部署指南](README.md)
- [Prometheus 告警规则](alert_rules.yml)
- [Grafana 仪表盘配置](grafana/dashboards/)
- [混合模式架构说明](../HYBRID_MODE.md)

