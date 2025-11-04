#!/bin/bash

# Redis 分布式锁与 TTL 控制演示脚本

REDIS_HOST="172.21.176.1"
REDIS_PORT="6379"

echo "=========================================="
echo "Redis 分布式锁与 TTL 控制演示"
echo "=========================================="
echo ""

# 检查 Redis 连接
if ! redis-cli -h $REDIS_HOST -p $REDIS_PORT ping > /dev/null 2>&1; then
    echo "❌ Redis 未连接"
    exit 1
fi

echo "✅ Redis 连接成功 ($REDIS_HOST:$REDIS_PORT)"
echo ""

# 清理旧数据
echo "清理旧测试数据..."
redis-cli -h $REDIS_HOST -p $REDIS_PORT <<EOF > /dev/null
DEL msg:processed:video_123:test_msg_1
DEL room:cooldown:video_123
DEL lock:broadcast:video_123
EOF
echo "✅ 清理完成"
echo ""

# ========== 演示1: 消息去重 ==========
echo "=========================================="
echo "演示1: 消息去重机制"
echo "=========================================="

MSG_ID="test_msg_1"
ROOM_ID="video_123"

echo "第1次处理消息 $MSG_ID:"
RESULT1=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET msg:processed:${ROOM_ID}:${MSG_ID} 1 NX EX 60)
if [ "$RESULT1" == "OK" ]; then
    echo "  ✅ 成功标记为已处理 (新消息)"
else
    echo "  ❌ 已处理过 (重复消息)"
fi

echo ""
echo "第2次处理相同消息 $MSG_ID (模拟重复):"
RESULT2=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET msg:processed:${ROOM_ID}:${MSG_ID} 1 NX EX 60)
if [ "$RESULT2" == "OK" ]; then
    echo "  ✅ 成功标记为已处理 (新消息)"
else
    echo "  ❌ 已处理过 (重复消息) - 符合预期！"
fi

echo ""
echo "查看去重记录:"
TTL=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT TTL msg:processed:${ROOM_ID}:${MSG_ID})
echo "  Key: msg:processed:${ROOM_ID}:${MSG_ID}"
echo "  TTL: ${TTL}s (60秒后自动过期)"
echo ""

# ========== 演示2: 房间冷却 ==========
echo "=========================================="
echo "演示2: 房间冷却机制（防热点）"
echo "=========================================="

echo "第1次广播房间 $ROOM_ID:"
COOLDOWN1=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET room:cooldown:${ROOM_ID} 1 NX EX 3)
if [ "$COOLDOWN1" == "OK" ]; then
    echo "  ✅ 允许广播，冷却期开始 (3秒)"
else
    echo "  ❌ 在冷却期内，跳过广播"
fi

echo ""
echo "第2次广播相同房间 (立即):"
COOLDOWN2=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET room:cooldown:${ROOM_ID} 1 NX EX 3)
if [ "$COOLDOWN2" == "OK" ]; then
    echo "  ✅ 允许广播，冷却期开始"
else
    echo "  ❌ 在冷却期内，跳过广播 - 符合预期！"
fi

echo ""
echo "查看冷却状态:"
TTL=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT TTL room:cooldown:${ROOM_ID})
echo "  Key: room:cooldown:${ROOM_ID}"
echo "  TTL: ${TTL}s (冷却中...)"

echo ""
echo "等待冷却期结束 (3秒)..."
sleep 3

echo "第3次广播 (3秒后):"
COOLDOWN3=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET room:cooldown:${ROOM_ID} 1 NX EX 3)
if [ "$COOLDOWN3" == "OK" ]; then
    echo "  ✅ 冷却期结束，允许广播！"
else
    echo "  ❌ 仍在冷却期"
fi
echo ""

# ========== 演示3: 分布式锁 ==========
echo "=========================================="
echo "演示3: 分布式锁机制"
echo "=========================================="

LOCK_KEY="lock:broadcast:${ROOM_ID}"
LOCK_VALUE_A="consumer_0_$(date +%s)"
LOCK_VALUE_B="consumer_1_$(date +%s)"

echo "Consumer 0 尝试获取锁:"
LOCK1=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET ${LOCK_KEY} ${LOCK_VALUE_A} NX EX 5)
if [ "$LOCK1" == "OK" ]; then
    echo "  ✅ Consumer 0 获取锁成功 (TTL: 5秒)"
else
    echo "  ❌ 锁已被占用"
fi

echo ""
echo "Consumer 1 尝试获取相同锁 (并发冲突):"
LOCK2=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT SET ${LOCK_KEY} ${LOCK_VALUE_B} NX EX 5)
if [ "$LOCK2" == "OK" ]; then
    echo "  ✅ Consumer 1 获取锁成功"
else
    echo "  ❌ Consumer 1 获取锁失败 - 符合预期！"
fi

echo ""
echo "查看锁状态:"
LOCK_HOLDER=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT GET ${LOCK_KEY})
TTL=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT TTL ${LOCK_KEY})
echo "  Lock Holder: $LOCK_HOLDER"
echo "  TTL: ${TTL}s"

echo ""
echo "Consumer 1 尝试错误释放锁 (值不匹配):"
# Lua 脚本：只能释放自己的锁
RELEASE1=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT EVAL "if redis.call('get', KEYS[1]) == ARGV[1] then return redis.call('del', KEYS[1]) else return 0 end" 1 ${LOCK_KEY} ${LOCK_VALUE_B})
if [ "$RELEASE1" == "1" ]; then
    echo "  ✅ 释放成功"
else
    echo "  ❌ 释放失败（值不匹配）- 符合预期！"
fi

echo ""
echo "Consumer 0 正确释放锁:"
RELEASE2=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT EVAL "if redis.call('get', KEYS[1]) == ARGV[1] then return redis.call('del', KEYS[1]) else return 0 end" 1 ${LOCK_KEY} ${LOCK_VALUE_A})
if [ "$RELEASE2" == "1" ]; then
    echo "  ✅ Consumer 0 释放锁成功！"
else
    echo "  ❌ 释放失败"
fi

echo ""
echo "验证锁已释放:"
EXISTS=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT EXISTS ${LOCK_KEY})
if [ "$EXISTS" == "0" ]; then
    echo "  ✅ 锁已被释放"
else
    echo "  ❌ 锁仍然存在"
fi
echo ""

# ========== 演示4: TTL 自动过期 ==========
echo "=========================================="
echo "演示4: TTL 自动过期（防死锁）"
echo "=========================================="

echo "模拟消费者崩溃场景:"
LOCK_VALUE_C="consumer_crashed_$(date +%s)"
redis-cli -h $REDIS_HOST -p $REDIS_PORT SET ${LOCK_KEY} ${LOCK_VALUE_C} EX 3 > /dev/null
echo "  Consumer 已获取锁（TTL 3秒）"
echo "  Consumer 模拟崩溃，未释放锁..."

echo ""
echo "等待 TTL 自动过期 (3秒)..."
for i in {3..1}; do
    TTL=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT TTL ${LOCK_KEY})
    echo "  倒计时: ${i}s (Redis TTL: ${TTL}s)"
    sleep 1
done

echo ""
echo "检查锁状态:"
EXISTS=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT EXISTS ${LOCK_KEY})
if [ "$EXISTS" == "0" ]; then
    echo "  ✅ 锁已自动过期，避免死锁！"
else
    echo "  ❌ 锁仍然存在"
fi
echo ""

# ========== 总结 ==========
echo "=========================================="
echo "✅ 演示完成！"
echo "=========================================="
echo ""
echo "核心机制验证:"
echo "  ✅ 消息去重: 60秒窗口，防止重复推送"
echo "  ✅ 房间冷却: 1-3秒限流，避免热点压力"
echo "  ✅ 分布式锁: 防止并发冲突，Lua 脚本安全释放"
echo "  ✅ TTL 过期: 自动清理，防止死锁和内存泄漏"
echo ""
echo "查看所有相关 Keys:"
echo "消息去重 Keys:"
redis-cli -h $REDIS_HOST -p $REDIS_PORT KEYS "msg:processed:*"
echo "房间冷却 Keys:"
redis-cli -h $REDIS_HOST -p $REDIS_PORT KEYS "room:cooldown:*"
echo "分布式锁 Keys:"
redis-cli -h $REDIS_HOST -p $REDIS_PORT KEYS "lock:broadcast:*"
echo ""
