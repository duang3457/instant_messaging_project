# 🎉 Prometheus + Grafana 监控系统

---

## 内容

### 1. **C++ 监控库** ✅

**位置**: `server/application/chat-room/monitoring/`

**核心文件**:
- `metrics_collector.h` - 监控指标封装类（头文件）
- `metrics_collector.cc` - 实现文件

**功能**:
- ✅ QPS 监控（Counter）
- ✅ 延迟监控（Histogram，支持 P50/P95/P99）
- ✅ 连接数监控（Gauge）
- ✅ 错误率监控（Counter）
- ✅ Kafka 消息监控（生产/消费）
- ✅ gRPC 调用监控（成功/失败）
- ✅ WebSocket 推送监控
- ✅ Redis 操作监控

**特点**:
- 单例模式，全局访问
- 线程安全
- RAII 风格延迟计时器
- 自动指标聚合和缓存

---

### 2. **监控基础设施** ✅

**位置**: `monitoring/`

**核心组件**:
```
monitoring/
├── docker-compose.yml          # 一键部署所有监控服务
├── prometheus.yml              # Prometheus 配置（抓取 3 个服务）
├── alert_rules.yml             # 8 大类告警规则
├── alertmanager.yml            # 告警管理（钉钉/邮件）
├── loki-config.yml             # 日志聚合配置
├── promtail-config.yml         # 日志采集配置
├── grafana/
│   ├── provisioning/
│   │   ├── datasources/        # 自动配置 Prometheus + Loki
│   │   └── dashboards/         # 自动加载仪表板
│   └── dashboards/
│       └── chatroom-overview.json  # 预定义监控面板
├── start.sh                    # 一键启动脚本
├── test.sh                     # 功能测试脚本
├── README.md                   # 使用说明
└── IMPLEMENTATION_GUIDE.md     # 实施指南
```

---

### 3. **已集成的服务** ✅

#### ✅ Comet 服务（chat-room）

**监控端口**: `:9091/metrics`

**已添加的监控埋点**:
```cpp
// 1. 构造函数 - 自动增加连接数
CWebSocketConn::CWebSocketConn() {
    MetricsCollector::GetInstance().IncrementActiveConnections();
}

// 2. 析构函数 - 自动减少连接数
CWebSocketConn::~CWebSocketConn() {
    MetricsCollector::GetInstance().DecrementActiveConnections();
}

// 3. 消息处理 - QPS + 延迟
int CWebSocketConn::handleClientMessages(Json::Value &root) {
    LatencyTimer timer(MetricsCollector::GetInstance(), "/ws/clientMessages");
    MetricsCollector::GetInstance().IncrementRequestCount("/ws/clientMessages", "WS");
    // ... 业务逻辑
}

// 4. 错误记录
if (error) {
    MetricsCollector::GetInstance().IncrementErrorCount("missing_fields", endpoint);
}

// 5. Redis 操作
MetricsCollector::GetInstance().IncrementRedisOp("store_message", success);

// 6. WebSocket 推送
MetricsCollector::GetInstance().IncrementWebSocketPush(room_id);
```

**main.cc 初始化**:
```cpp
// 初始化监控（:9091 端口）
MetricsCollector::GetInstance().Initialize("0.0.0.0:9091", "comet");
```

---

### 4. **告警规则** ✅

已配置 **8 大类共 15 个告警规则**:

| 类别 | 告警名称 | 阈值 | 持续时间 |
|------|----------|------|----------|
| 可用性 | ServiceDown | up == 0 | 1分钟 |
| QPS | HighQPS | rate > 1000 req/s | 2分钟 |
| QPS | QPSSurge | 增长 > 50% | 1分钟 |
| 延迟 | HighP99Latency | P99 > 1秒 | 2分钟 |
| 延迟 | HighP95Latency | P95 > 500ms | 5分钟 |
| 错误率 | HighErrorRate | > 5% | 2分钟 |
| 错误率 | HighErrorCount | > 10 次/分钟 | 1分钟 |
| 连接 | HighConnectionCount | > 10000 | 2分钟 |
| 连接 | ConnectionDrop | 下降 > 50% | 1分钟 |
| Kafka | KafkaLag | 积压 > 1000 | 5分钟 |
| WebSocket | HighWebSocketPushFailure | 成功率 < 95% | 2分钟 |
| gRPC | HighGrpcFailureRate | 失败率 > 5% | 2分钟 |
| Redis | HighRedisFailureRate | 失败率 > 1% | 2分钟 |

---

### 5. **Grafana 仪表板** ✅

**预定义面板**:
1. **QPS** - 每秒请求数（按服务/端点分组）
2. **错误率** - 实时错误率（阈值着色）
3. **P99 延迟** - 99分位延迟（按端点）
4. **活跃连接数** - Gauge 仪表盘
5. **Kafka 消息流** - 生产/消费速率

**访问地址**: http://localhost:3000
- 用户名: `admin`
- 密码: `admin123`

---

### 6. **日志聚合** ✅

**Loki + Promtail 配置完成**

**支持的日志查询**:
```logql
# 查看所有 Comet 日志
{service="comet"}

# 查看错误日志
{service="comet"} |= "ERROR"

# 统计错误数量
sum(count_over_time({service="comet"} |= "ERROR" [5m]))
```

---

## 🚀 快速开始（3 步）

### 步骤 1: 启动监控系统

```bash
cd /home/yang/chatroom/monitoring
./start.sh
```

或手动启动：
```bash
docker compose up -d
```

### 步骤 2: 启动 Comet 服务

```bash
cd /home/yang/chatroom/server/build
./chat-room
```

### 步骤 3: 访问监控面板

打开浏览器访问: **http://localhost:3000**

- 用户名: `admin`
- 密码: `admin123`

导航到 **Dashboards** → **聊天室系统监控 - Overview**

---

## 📊 核心指标速查

### 1️⃣ QPS（每秒请求数）

**PromQL**:
```promql
# 总体 QPS
rate(http_requests_total[1m])

# 按服务分组
sum(rate(http_requests_total[1m])) by (service)
```

---

### 2️⃣ 延迟（Latency）

**PromQL**:
```promql
# P99 延迟（微秒）
histogram_quantile(0.99, 
  sum(rate(http_request_duration_microseconds_bucket[5m])) by (le, service)
)

# 平均延迟
rate(http_request_duration_microseconds_sum[5m]) 
/ 
rate(http_request_duration_microseconds_count[5m])
```

---

### 3️⃣ 活跃连接数

**PromQL**:
```promql
active_connections{service="comet"}
```

---

### 4️⃣ 错误率

**PromQL**:
```promql
# 错误率（百分比）
(
  sum(rate(errors_total[5m])) by (service)
  / 
  sum(rate(http_requests_total[5m])) by (service)
) * 100
```

---

### 5️⃣ WebSocket 推送

**PromQL**:
```promql
# 推送速率
rate(websocket_messages_pushed_total[1m])

# 按房间统计
sum(rate(websocket_messages_pushed_total[5m])) by (room_id)
```

---

## 🔔 告警配置

### 配置钉钉告警

1. 编辑 `monitoring/alertmanager.yml`:

```yaml
webhook_configs:
  - url: 'https://oapi.dingtalk.com/robot/send?access_token=YOUR_TOKEN'
```

2. 获取钉钉 Webhook Token:
   - 打开钉钉群 → 群设置 → 智能群助手
   - 添加机器人 → 自定义
   - 复制 access_token

3. 重启 Alertmanager:
```bash
cd /home/yang/chatroom/monitoring
docker compose restart alertmanager
```

---

## 🧪 功能测试

运行测试脚本验证所有功能：

```bash
cd /home/yang/chatroom/monitoring
./test.sh
```

测试内容：
- ✅ Prometheus 健康检查
- ✅ Grafana 健康检查
- ✅ Comet /metrics 端点
- ✅ 关键指标存在性验证
- ✅ PromQL 查询测试
- ✅ 告警规则加载

---

## 📈 实时查看指标

### 方法 1: 直接访问 Metrics 端点

```bash
# 查看原始指标
curl http://localhost:9091/metrics

# 过滤特定指标
curl http://localhost:9091/metrics | grep http_requests_total
```

### 方法 2: Prometheus UI

访问 http://localhost:9090

输入 PromQL 查询，例如：
```promql
rate(http_requests_total[1m])
```

### 方法 3: Grafana Explore

访问 http://localhost:3000 → 左侧菜单 → **Explore**

---

## 🛠️ 下一步工作

### ⏳ 待集成服务

#### Logic 服务监控

```bash
# 1. 复制监控代码
cp -r /home/yang/chatroom/server/application/chat-room/monitoring \
      /home/yang/chatroom/server/application/logic/

# 2. 在 Logic 的 main.cc 中初始化
MetricsCollector::GetInstance().Initialize("0.0.0.0:9092", "logic");

# 3. 在 Kafka 生产代码中添加
MetricsCollector::GetInstance().IncrementKafkaProduced("my-topic");
```

#### Job 服务监控

```bash
# 1. 复制监控代码
cp -r /home/yang/chatroom/server/application/chat-room/monitoring \
      /home/yang/chatroom/server/application/job/

# 2. 在 Job 的 main.cc 中初始化
MetricsCollector::GetInstance().Initialize("0.0.0.0:9093", "job");

# 3. 在 Kafka 消费代码中添加
MetricsCollector::GetInstance().IncrementKafkaConsumed("my-topic");

# 4. 在 gRPC 调用代码中添加
MetricsCollector::GetInstance().IncrementGrpcCall("BroadcastRoom", success);
```

### 💡 可选增强

1. **系统资源监控**: 安装 node_exporter 监控 CPU/内存/磁盘
2. **Kafka 监控**: 使用 kafka_exporter 监控 Kafka 集群
3. **Redis 监控**: 使用 redis_exporter 监控 Redis
4. **MySQL 监控**: 使用 mysqld_exporter 监控数据库

---

## 📚 文档索引

- **README.md** - 快速开始和基本使用
- **IMPLEMENTATION_GUIDE.md** - 详细实施指南
- **alert_rules.yml** - 所有告警规则配置
- **prometheus.yml** - Prometheus 配置
- **alertmanager.yml** - 告警管理配置

---

## 🎯 监控体系完整度

```
应用层监控:
├── Comet (WebSocket)     ✅ 已完成 (:9091/metrics)
├── Logic (HTTP API)      ⏳ 待集成 (:9092/metrics)
└── Job (Kafka Worker)    ⏳ 待集成 (:9093/metrics)

基础设施:
├── Prometheus            ✅ 已部署 (:9090)
├── Grafana               ✅ 已部署 (:3000)
├── Alertmanager          ✅ 已部署 (:9093)
├── Loki                  ✅ 已部署 (:3100)
└── Promtail              ✅ 已部署

告警系统:
├── 告警规则              ✅ 8 大类 15 个规则
├── 钉钉通知              ⏳ 需配置 Token
└── 邮件通知              ⏳ 需配置 SMTP

日志聚合:
├── 日志采集              ✅ Promtail 配置完成
└── 日志查询              ✅ Loki + Grafana

可视化:
├── 预定义 Dashboard      ✅ 5 个核心面板
└── 自定义查询            ✅ Explore 可用
```

---


## 🎉 总结

✅ **实时监控**: QPS、延迟、连接数、错误率
✅ **可视化**: Grafana Dashboard 美观展示
✅ **告警**: 多种告警规则 + 钉钉/邮件通知
✅ **日志聚合**: Loki 统一管理所有日志
✅ **一键部署**: Docker Compose 简化运维
✅ **完整文档**: 3 份文档覆盖所有场景


访问 http://localhost:3000
