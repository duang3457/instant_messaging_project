#!/bin/bash

# Redis 路由测试数据初始化脚本

echo "=========================================="
echo "Redis 路由功能测试数据初始化"
echo "=========================================="

# Redis 连接信息（Windows 机器）
REDIS_HOST="172.21.176.1"
REDIS_PORT="6379"

# 检查 Redis 是否运行
if ! redis-cli -h $REDIS_HOST -p $REDIS_PORT ping > /dev/null 2>&1; then
    echo "❌ Redis 未运行，请先启动 Redis"
    exit 1
fi

echo "✅ Redis 连接成功"
echo ""

# 清理旧数据
echo "清理旧测试数据..."
redis-cli -h $REDIS_HOST -p $REDIS_PORT <<EOF > /dev/null
DEL room:connections:video_123
DEL connection:info:conn_1
DEL connection:info:conn_2
DEL connection:info:conn_3
DEL connection:info:conn_4
DEL user:online:user_123
DEL user:online:user_456
DEL user:online:user_789
DEL user:online:user_999
EOF

echo "✅ 旧数据清理完成"
echo ""

# 场景1: 单个 Comet 节点，多个用户
echo "=========================================="
echo "场景1: 单个 Comet 节点 (localhost:50051)"
echo "=========================================="

redis-cli -h $REDIS_HOST -p $REDIS_PORT <<EOF
SADD room:connections:video_123 conn_1
HMSET connection:info:conn_1 comet_id localhost:50051 user_id user_123 room_id video_123
SET user:online:user_123 conn_1

SADD room:connections:video_123 conn_2
HMSET connection:info:conn_2 comet_id localhost:50051 user_id user_456 room_id video_123
SET user:online:user_456 conn_2
EOF

echo "✅ 用户 user_123 (conn_1) -> localhost:50051"
echo "✅ 用户 user_456 (conn_2) -> localhost:50051"
echo ""

# 场景2: 多个 Comet 节点
echo "=========================================="
echo "场景2: 添加第二个 Comet 节点 (localhost:50052)"
echo "=========================================="

redis-cli -h $REDIS_HOST -p $REDIS_PORT <<EOF
SADD room:connections:video_123 conn_3
HMSET connection:info:conn_3 comet_id localhost:50052 user_id user_789 room_id video_123
SET user:online:user_789 conn_3

SADD room:connections:video_123 conn_4
HMSET connection:info:conn_4 comet_id localhost:50052 user_id user_999 room_id video_123
SET user:online:user_999 conn_4
EOF

echo "✅ 用户 user_789 (conn_3) -> localhost:50052"
echo "✅ 用户 user_999 (conn_4) -> localhost:50052"
echo ""

# 验证数据
echo "=========================================="
echo "数据验证"
echo "=========================================="

echo "房间 video_123 的连接数："
CONN_COUNT=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SCARD room:connections:video_123)
echo "  总连接数: $CONN_COUNT"
echo ""

echo "房间内所有连接："
redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:video_123
echo ""

echo "连接详细信息："
for conn_id in conn_1 conn_2 conn_3 conn_4; do
    echo "  $conn_id:"
    redis-cli -h $REDIS_HOST -p $REDIS_PORT HGETALL connection:info:$conn_id | while read -r field; read -r value; do
        echo "    $field: $value"
    done
    echo ""
done

echo "用户在线状态："
for user_id in user_123 user_456 user_789 user_999; do
    ONLINE=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT EXISTS user:online:$user_id)
    CONN=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT GET user:online:$user_id)
    if [ "$ONLINE" == "1" ]; then
        echo "  ✅ $user_id: 在线 (连接: $CONN)"
    else
        echo "  ❌ $user_id: 离线"
    fi
done
echo ""

# 按 Comet 节点统计
echo "=========================================="
echo "按 Comet 节点统计"
echo "=========================================="

echo "localhost:50051 节点:"
redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:video_123 | while read conn_id; do
    COMET=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id comet_id)
    if [ "$COMET" == "localhost:50051" ]; then
        USER=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id user_id)
        echo "  - $conn_id ($USER)"
    fi
done
echo ""

echo "localhost:50052 节点:"
redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:video_123 | while read conn_id; do
    COMET=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id comet_id)
    if [ "$COMET" == "localhost:50052" ]; then
        USER=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id user_id)
        echo "  - $conn_id ($USER)"
    fi
done
echo ""

echo "=========================================="
echo "✅ 测试数据初始化完成！"
echo "=========================================="
echo ""
echo "下一步："
echo "1. 启动 Comet 服务: cd server/build/application/chat-room && ./comet"
echo "2. 启动 Job 服务: cd server/build/application/job && ./job"
echo "3. 发送测试消息到 Kafka，观察 Job 的路由分发日志"
echo ""
