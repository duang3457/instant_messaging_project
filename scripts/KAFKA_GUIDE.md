# Kafka服务快速使用指南

### 📊 状态
- **Zookeeper**: 端口 2181
- **Kafka**: 端口 9092
- **Topic**: my-topic (3个分区)
- **安装目录**: ~/kafka

## 🎮 Kafka管理命令

使用管理脚本 `scripts/kafka.sh`:

```bash
# 查看服务状态
./kafka.sh status

# 启动Kafka（Zookeeper + Kafka）
./kafka.sh start

# 停止Kafka
./kafka.sh stop

# 重启Kafka
./kafka.sh restart

# 查看Kafka日志
./kafka.sh logs kafka

# 查看Zookeeper日志
./kafka.sh logs zk

# 列出所有Topics
./kafka.sh list-topics

# 创建新Topic
./kafka.sh create-topic my-new-topic

# 删除Topic
./kafka.sh delete-topic my-old-topic

# 测试生产者（发送消息）
./kafka.sh producer my-topic

# 测试消费者（接收消息）
./kafka.sh consumer my-topic
```

## 🚀 现在可以启动Job服务

Kafka已经准备就绪，可以启动Job服务了：

```bash
cd server/application/job
./start_job.sh
```

或者直接运行：

```bash
cd server/build/application/job
./job
```

## 🧪 测试Kafka消息流

### 终端1: 启动Job服务
```bash
cd server/build/application/job
./job
```

### 终端2: 发送测试消息
```bash
cd scripts
./kafka.sh producer my-topic

# 然后输入消息并按Enter
```

### 终端3: 消费消息（可选）
```bash
cd scripts
./kafka.sh consumer my-topic
```

## 📝 Kafka配置信息

- **Broker地址**: localhost:9092
- **Zookeeper地址**: localhost:2181
- **数据目录**: ~/kafka/data/
- **日志目录**: ~/kafka/logs/
- **配置文件**: ~/kafka/config/

## 🔧 常见问题

### 1. 如何检查Kafka是否正常运行？
```bash
./kafka.sh status
```

### 2. 如何查看Kafka日志？
```bash
./kafka.sh logs kafka
# 或
tail -f ~/kafka/logs/kafka.log
```

### 3. 如何重启Kafka？
```bash
./kafka.sh restart
```

### 4. Kafka启动失败怎么办？
检查日志：
```bash
cat ~/kafka/logs/kafka.log
cat ~/kafka/logs/zookeeper.log
```

确保端口没有被占用：
```bash
lsof -i :9092  # Kafka
lsof -i :2181  # Zookeeper
```

### 5. 如何完全清理Kafka数据？
```bash
./kafka.sh stop
rm -rf ~/kafka/data/*
./kafka.sh start
```

## 💡 提示

- Kafka会后台运行，重启系统后需要手动启动
- 可以将启动命令添加到开机脚本中
- 建议定期清理旧日志文件