#!/bin/bash

# Job服务启动脚本
# Job服务负责从Kafka消费消息并通过gRPC转发到Comet服务

cd "$(dirname "$0")/../build/application/job"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}=== 聊天室Job服务启动脚本 ===${NC}"

# 检查job程序是否存在
if [[ ! -f "./job" ]]; then
    echo -e "${RED}错误: job程序不存在${NC}"
    echo "请先编译job服务:"
    echo "  cd ../../"
    echo "  cmake .. -DENABLE_RPC=ON"
    echo "  make job"
    exit 1
fi

echo -e "${BLUE}检查依赖服务...${NC}"

# 检查Kafka是否运行
echo -n "检查Kafka服务... "
if nc -z localhost 9092 2>/dev/null; then
    echo -e "${GREEN}✓ Kafka运行中${NC}"
else
    echo -e "${RED}✗ Kafka未运行${NC}"
    echo -e "${YELLOW}请先启动Kafka服务：${NC}"
    echo "  1. 启动Zookeeper: bin/zookeeper-server-start.sh config/zookeeper.properties"
    echo "  2. 启动Kafka: bin/kafka-server-start.sh config/server.properties"
    read -p "是否继续启动job服务? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# 检查Comet服务(gRPC)是否运行
echo -n "检查Comet服务(gRPC)... "
if nc -z localhost 50051 2>/dev/null; then
    echo -e "${GREEN}✓ Comet服务运行中${NC}"
else
    echo -e "${YELLOW}⚠ Comet服务(端口50051)未运行${NC}"
    echo -e "${YELLOW}请确保Comet服务已启动${NC}"
fi

echo ""
echo -e "${BLUE}Job服务配置信息:${NC}"
echo "  Kafka地址: localhost:9092"
echo "  消费主题: my-topic"
echo "  消费者组: my_consumer_group"
echo "  Comet地址: localhost:50051"
echo ""

# 启动job服务
echo -e "${GREEN}启动Job服务...${NC}"
./job

# 如果程序退出
echo -e "${YELLOW}Job服务已停止${NC}"