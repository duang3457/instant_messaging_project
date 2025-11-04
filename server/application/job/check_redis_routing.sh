#!/bin/bash

# 快速查看 Redis 路由数据的脚本

REDIS_HOST="172.21.176.1"
REDIS_PORT="6379"

echo "=========================================="
echo "Redis 路由数据查看工具"
echo "=========================================="
echo ""

# 检查参数
if [ $# -eq 1 ]; then
    ROOM_ID=$1
else
    ROOM_ID="video_123"
fi

echo "查询房间: $ROOM_ID"
echo ""

# 1. 查询房间连接数
echo "1️⃣  房间连接统计"
echo "----------------------------------------"
CONN_COUNT=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SCARD room:connections:$ROOM_ID)
echo "总连接数: $CONN_COUNT"
echo ""

# 2. 显示所有连接ID
echo "2️⃣  房间内的连接列表"
echo "----------------------------------------"
redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:$ROOM_ID
echo ""

# 3. 按 Comet 节点分组显示
echo "3️⃣  按 Comet 节点分组"
echo "----------------------------------------"

# 使用数组存储不同的 comet 节点
declare -A comet_nodes

redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:$ROOM_ID | while read conn_id; do
    if [ -n "$conn_id" ]; then
        COMET=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id comet_id)
        USER=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id user_id)
        echo "[$COMET] $conn_id -> $USER"
    fi
done
echo ""

# 4. 详细连接信息
echo "4️⃣  详细连接信息"
echo "----------------------------------------"
redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:$ROOM_ID | while read conn_id; do
    if [ -n "$conn_id" ]; then
        echo "连接: $conn_id"
        redis-cli -h $REDIS_HOST -p $REDIS_PORT HGETALL connection:info:$conn_id | while read -r field; read -r value; do
            echo "  $field = $value"
        done
        echo ""
    fi
done

# 5. 在线用户统计
echo "5️⃣  在线用户检查"
echo "----------------------------------------"
# 获取房间内所有用户ID
USER_IDS=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SMEMBERS room:connections:$ROOM_ID | while read conn_id; do
    redis-cli -h $REDIS_HOST -p $REDIS_PORT HGET connection:info:$conn_id user_id
done | sort -u)

ONLINE_COUNT=0
OFFLINE_COUNT=0

for user_id in $USER_IDS; do
    if [ -n "$user_id" ]; then
        ONLINE=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT EXISTS user:online:$user_id)
        if [ "$ONLINE" == "1" ]; then
            CONN=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT GET user:online:$user_id)
            echo "✅ $user_id (连接: $CONN)"
            ((ONLINE_COUNT++))
        else
            echo "❌ $user_id (离线)"
            ((OFFLINE_COUNT++))
        fi
    fi
done

echo ""
echo "在线: $ONLINE_COUNT | 离线: $OFFLINE_COUNT"
echo ""

echo "=========================================="
echo "提示："
echo "  查看其他房间: $0 <room_id>"
echo "  示例: $0 video_456"
echo "=========================================="
