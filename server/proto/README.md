# Proto 文件管理说明

## 目录结构

所有 `.proto` 文件统一存放在 `server/proto/` 目录下：
 
```
server/proto/
├── ChatRoom.Comet.proto      # Comet 服务的 gRPC 定义
├── ChatRoom.Protocol.proto   # 通用协议消息定义
├── ChatRoom.Job.proto         # Job 消息队列相关定义
└── CMakeLists.txt            # 生成共享 proto 库的构建脚本
```

## 构建系统

### 1. Proto 库生成

`server/proto/CMakeLists.txt` 负责：
- 使用 `protoc` 和 `grpc_cpp_plugin` 生成 C++ 代码
- 创建 `chatroom_proto` 静态库
- 链接 protobuf 和 gRPC 依赖

生成的文件位于：`server/build/proto/`

### 2. 服务链接

各个服务（job、logic、chat-room）通过链接 `chatroom_proto` 库来使用 proto 定义：

```cmake
TARGET_LINK_LIBRARIES(job chatroom_proto ...)
TARGET_LINK_LIBRARIES(logic chatroom_proto ...)
TARGET_LINK_LIBRARIES(chat-room chatroom_proto ...)
```

## Proto 文件说明

### ChatRoom.Protocol.proto
- 定义基础协议消息 `Proto`
- 包含版本号、操作码、序列号和消息体
- 被其他 proto 文件导入使用

### ChatRoom.Comet.proto
- 定义 Comet 服务的 gRPC 接口
- 包含 4 个 RPC 方法：
  - `PushMsg`: 推送消息给指定用户
  - `Broadcast`: 广播给所有用户
  - `BroadcastRoom`: 广播给指定房间
  - `Rooms`: 获取所有房间列表

### ChatRoom.Job.proto
- 定义 Kafka 消息队列的消息格式
- `PushMsg` 支持三种推送类型：PUSH、ROOM、BROADCAST

## 编译说明

### 启用 RPC 功能

```bash
cd server/build
cmake -DENABLE_RPC=ON ..
make -j4
```
