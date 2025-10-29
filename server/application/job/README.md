# Job服务运行说明

## 服务介绍

Job服务是聊天室系统的消息分发服务，负责：
1. 从Kafka消费消息队列中的消息
2. 通过gRPC调用Comet服务进行消息广播
3. 实现消息的异步处理和分发

## 架构图
```
Logic服务 -> Kafka -> Job服务 -> (gRPC) -> Comet服务 -> WebSocket客户端
```

## 依赖服务

### 1. Kafka服务
Job服务需要连接到Kafka消息队列

**启动Zookeeper:**
```bash
cd kafka目录
bin/zookeeper-server-start.sh config/zookeeper.properties
```

**启动Kafka:**
```bash
bin/kafka-server-start.sh config/server.properties
```

**创建Topic (首次运行需要):**
```bash
bin/kafka-topics.sh --create \
  --bootstrap-server localhost:9092 \
  --topic my-topic \
  --partitions 1 \
  --replication-factor 1
```

**查看Topic列表:**
```bash
bin/kafka-topics.sh --list --bootstrap-server localhost:9092
```

### 2. Comet服务 (gRPC服务)
Job服务通过gRPC与Comet服务通信，确保Comet服务在50051端口运行

## 编译Job服务

```bash
cd server/build
cmake .. -DENABLE_RPC=ON
make job
```

编译成功后，可执行文件位于: `server/build/application/job/job`

## 运行Job服务

### 方式1: 使用启动脚本 (推荐)
```bash
cd server/application/job
./start_job.sh
```

### 方式2: 直接运行
```bash
cd server/build/application/job
./job
```

## 配置说明

当前Job服务使用硬编码配置，可在`main.cc`中修改:

```cpp
// Kafka配置
KafkaConsumer consumer("localhost:9092", "my-topic");

// Comet gRPC地址
client_ = std::make_unique<CometClient>("localhost:50051");
```

## 消息格式

Job服务消费的Kafka消息格式 (Protobuf):

```protobuf
message PushMsg {
    enum Type {
        PUSH = 0;       // 推送给指定用户
        ROOM = 1;       // 推送给指定房间  
        BROADCAST = 2;  // 广播给所有人
    }
    
    Type type = 1;
    int32 operation = 2;
    string room = 5;          // 房间ID
    bytes msg = 7;            // 消息内容
}
```

## 测试Job服务

### 1. 生产消息到Kafka

创建一个简单的Kafka生产者测试:

```bash
# 使用Kafka自带的控制台生产者
bin/kafka-console-producer.sh \
  --bootstrap-server localhost:9092 \
  --topic my-topic

# 然后输入测试消息
```

### 2. 查看Job服务日志

Job服务会输出接收到的消息信息:
```
Received PushMsg:
  Type: 1
  Operation: 4
  roomId: general
  msg: <消息内容>
```

### 3. 验证消息转发

检查Comet服务是否收到gRPC调用并广播消息到WebSocket客户端

## 故障排查

### 问题1: 无法连接Kafka
```
错误: Failed to initialize Kafka consumer
```

**解决方案:**
- 检查Kafka是否在localhost:9092运行
- 检查Topic是否存在
- 查看Kafka日志

### 问题2: 无法连接Comet服务
```
错误: RPC failed: <错误信息>
```

**解决方案:**
- 确认Comet服务在localhost:50051运行
- 检查gRPC服务是否正常
- 验证网络连接

### 问题3: Proto解析失败
```
错误: Failed to parse message as PushMsg
```

**解决方案:**
- 确认Kafka消息格式正确
- 检查Proto版本兼容性
- 验证序列化方式

## 监控和运维

### 查看Kafka消费组状态
```bash
bin/kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --group my_consumer_group \
  --describe
```

### 查看消息堆积
```bash
bin/kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --group my_consumer_group \
  --describe | grep LAG
```

### 性能调优

1. **增加消费线程数**
   修改`main.cc`中的线程池大小:
   ```cpp
   threadPool.start(4); // 调整线程数
   ```

2. **调整Kafka批量大小**
   修改KafkaConsumer配置

3. **优化gRPC连接池**
   使用连接池复用gRPC连接

## 日志级别

Job服务使用Muduo的日志系统，可通过环境变量设置日志级别:
```bash
export MUDUO_LOG_LEVEL=INFO  # TRACE, DEBUG, INFO, WARN, ERROR
./job
```

## 进程管理

### 后台运行
```bash
nohup ./job > job.log 2>&1 &
```

### 使用systemd管理
创建systemd服务文件 `/etc/systemd/system/chatroom-job.service`

## 相关服务

- Logic服务: 生产消息到Kafka
- Comet服务: 接收gRPC调用并广播消息
- Chat-room服务: WebSocket服务器