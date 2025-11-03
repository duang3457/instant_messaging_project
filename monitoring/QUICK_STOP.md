# 监控服务快速操作指南

## 🚀 启动监控服务

```bash
cd ~/chatroom/monitoring
./start.sh
```

**服务包括**：
- Prometheus (9090)
- Grafana (3000) - 用户名: admin, 密码: admin123
- Alertmanager (9093)
- Loki (3100)
- Promtail (日志收集)

---

## 🛑 停止监控服务

### 方式 1：使用监控停止脚本（推荐）

```bash
cd ~/chatroom/monitoring
./stop.sh
```

**特性**：
- 会提示确认是否停止
- 显示当前运行的服务
- 保留数据卷（Dashboard 和历史数据）

---

### 方式 2：直接使用 Docker Compose

```bash
cd ~/chatroom/monitoring

# 停止并删除容器（保留数据）
docker-compose down

# 或使用 Docker Compose V2 插件
docker compose down
```

---

### 方式 3：停止并删除所有数据

```bash
cd ~/chatroom/monitoring

# ⚠️ 警告：这会删除所有 Grafana Dashboard 和 Prometheus 历史数据
docker-compose down -v
```

---

## 📊 查看服务状态

```bash
cd ~/chatroom/monitoring

# 查看运行中的容器
docker-compose ps

# 查看服务日志
docker-compose logs

# 实时查看日志
docker-compose logs -f

# 查看特定服务的日志
docker-compose logs -f prometheus
docker-compose logs -f grafana
```

---

## 🔄 重启单个服务

```bash
cd ~/chatroom/monitoring

# 重启 Prometheus
docker-compose restart prometheus

# 重启 Grafana
docker-compose restart grafana

# 重启 Loki
docker-compose restart loki
```

---

## 🧹 清理和维护

### 清理未使用的 Docker 资源

```bash
# 清理停止的容器
docker container prune

# 清理未使用的镜像
docker image prune

# 清理未使用的卷
docker volume prune

# 一键清理所有未使用资源
docker system prune
```

---

### 查看监控数据卷

```bash
# 列出所有卷
docker volume ls | grep monitoring

# 查看卷详情
docker volume inspect monitoring_prometheus_data
docker volume inspect monitoring_grafana_data
```

---

## 📍 快捷命令总结

| 操作 | 命令 |
|------|------|
| 启动监控 | `cd ~/chatroom/monitoring && ./start.sh` |
| 停止监控 | `cd ~/chatroom/monitoring && ./stop.sh` |
| 查看状态 | `cd ~/chatroom/monitoring && docker-compose ps` |
| 查看日志 | `cd ~/chatroom/monitoring && docker-compose logs -f` |
| 重启服务 | `cd ~/chatroom/monitoring && docker-compose restart` |
| 删除所有 | `cd ~/chatroom/monitoring && docker-compose down -v` |

---

## 🔧 集成到全局脚本

监控服务已集成到全局 `stop_all.sh` 脚本中：

```bash
# 运行全局停止脚本
cd ~/chatroom
./stop_all.sh

# 会提示：是否同时停止监控服务 (Prometheus/Grafana)? (y/n)
# 输入 y 即可同时停止监控
```

---

## 📚 相关文档

- [监控系统完整文档](README.md)
- [实施指南](IMPLEMENTATION_GUIDE.md)
- [交付文档](DELIVERY.md)

---

**提示**：停止监控服务不会影响应用服务（Comet、Logic、Job），它们可以独立运行。
