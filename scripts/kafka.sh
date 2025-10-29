#!/bin/bash

# Kafka服务管理脚本

KAFKA_DIR="$HOME/kafka"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 检查Kafka是否已安装
if [[ ! -d "$KAFKA_DIR" ]]; then
    echo -e "${RED}错误: Kafka未安装${NC}"
    echo "请先运行安装脚本: ./setup_kafka.sh"
    exit 1
fi

# 启动Zookeeper
start_zookeeper() {
    echo -e "${BLUE}启动Zookeeper...${NC}"
    
    if lsof -i :2181 >/dev/null 2>&1; then
        echo -e "${YELLOW}Zookeeper已在运行${NC}"
        return 0
    fi
    
    cd "$KAFKA_DIR"
    nohup bin/zookeeper-server-start.sh config/zookeeper.properties > logs/zookeeper.log 2>&1 &
    sleep 3
    
    if lsof -i :2181 >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Zookeeper启动成功${NC}"
    else
        echo -e "${RED}✗ Zookeeper启动失败${NC}"
        return 1
    fi
}

# 启动Kafka
start_kafka() {
    echo -e "${BLUE}启动Kafka...${NC}"
    
    if lsof -i :9092 >/dev/null 2>&1; then
        echo -e "${YELLOW}Kafka已在运行${NC}"
        return 0
    fi
    
    cd "$KAFKA_DIR"
    nohup bin/kafka-server-start.sh config/server.properties > logs/kafka.log 2>&1 &
    sleep 5
    
    if lsof -i :9092 >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Kafka启动成功${NC}"
    else
        echo -e "${RED}✗ Kafka启动失败${NC}"
        return 1
    fi
}

# 停止Kafka
stop_kafka() {
    echo -e "${BLUE}停止Kafka...${NC}"
    
    if ! lsof -i :9092 >/dev/null 2>&1; then
        echo -e "${YELLOW}Kafka未在运行${NC}"
        return 0
    fi
    
    cd "$KAFKA_DIR"
    bin/kafka-server-stop.sh
    sleep 3
    echo -e "${GREEN}✓ Kafka已停止${NC}"
}

# 停止Zookeeper
stop_zookeeper() {
    echo -e "${BLUE}停止Zookeeper...${NC}"
    
    if ! lsof -i :2181 >/dev/null 2>&1; then
        echo -e "${YELLOW}Zookeeper未在运行${NC}"
        return 0
    fi
    
    cd "$KAFKA_DIR"
    bin/zookeeper-server-stop.sh
    sleep 3
    echo -e "${GREEN}✓ Zookeeper已停止${NC}"
}

# 查看状态
status() {
    echo -e "${BLUE}=== Kafka服务状态 ===${NC}"
    
    if lsof -i :2181 >/dev/null 2>&1; then
        ZK_PID=$(lsof -ti :2181)
        echo -e "${GREEN}✓ Zookeeper运行中${NC} (PID: $ZK_PID, 端口: 2181)"
    else
        echo -e "${RED}✗ Zookeeper未运行${NC}"
    fi
    
    if lsof -i :9092 >/dev/null 2>&1; then
        KAFKA_PID=$(lsof -ti :9092)
        echo -e "${GREEN}✓ Kafka运行中${NC} (PID: $KAFKA_PID, 端口: 9092)"
    else
        echo -e "${RED}✗ Kafka未运行${NC}"
    fi
}

# 查看日志
logs() {
    local service=${1:-kafka}
    
    if [[ "$service" == "zookeeper" ]] || [[ "$service" == "zk" ]]; then
        echo -e "${BLUE}Zookeeper日志:${NC}"
        tail -f "$KAFKA_DIR/logs/zookeeper.log"
    else
        echo -e "${BLUE}Kafka日志:${NC}"
        tail -f "$KAFKA_DIR/logs/kafka.log"
    fi
}

# 列出Topics
list_topics() {
    echo -e "${BLUE}=== Kafka Topics ===${NC}"
    cd "$KAFKA_DIR"
    bin/kafka-topics.sh --list --bootstrap-server localhost:9092
}

# 创建Topic
create_topic() {
    local topic_name=$1
    
    if [[ -z "$topic_name" ]]; then
        echo -e "${RED}错误: 请提供Topic名称${NC}"
        echo "用法: $0 create-topic <topic-name>"
        return 1
    fi
    
    cd "$KAFKA_DIR"
    bin/kafka-topics.sh --create \
        --bootstrap-server localhost:9092 \
        --topic "$topic_name" \
        --partitions 3 \
        --replication-factor 1
    
    echo -e "${GREEN}✓ Topic '$topic_name' 创建成功${NC}"
}

# 删除Topic
delete_topic() {
    local topic_name=$1
    
    if [[ -z "$topic_name" ]]; then
        echo -e "${RED}错误: 请提供Topic名称${NC}"
        echo "用法: $0 delete-topic <topic-name>"
        return 1
    fi
    
    cd "$KAFKA_DIR"
    bin/kafka-topics.sh --delete \
        --bootstrap-server localhost:9092 \
        --topic "$topic_name"
    
    echo -e "${GREEN}✓ Topic '$topic_name' 已删除${NC}"
}

# 测试生产者
test_producer() {
    local topic=${1:-my-topic}
    echo -e "${BLUE}启动测试生产者 (Topic: $topic)${NC}"
    echo "输入消息后按Enter发送，Ctrl+C退出"
    cd "$KAFKA_DIR"
    bin/kafka-console-producer.sh --bootstrap-server localhost:9092 --topic "$topic"
}

# 测试消费者
test_consumer() {
    local topic=${1:-my-topic}
    echo -e "${BLUE}启动测试消费者 (Topic: $topic)${NC}"
    cd "$KAFKA_DIR"
    bin/kafka-console-consumer.sh \
        --bootstrap-server localhost:9092 \
        --topic "$topic" \
        --from-beginning
}

# 显示帮助
show_help() {
    echo "Kafka服务管理脚本"
    echo ""
    echo "用法: $0 <command> [options]"
    echo ""
    echo "命令:"
    echo "  start              - 启动Kafka和Zookeeper"
    echo "  stop               - 停止Kafka和Zookeeper"
    echo "  restart            - 重启Kafka和Zookeeper"
    echo "  status             - 查看服务状态"
    echo "  logs [kafka|zk]    - 查看日志"
    echo "  list-topics        - 列出所有Topics"
    echo "  create-topic <名称> - 创建Topic"
    echo "  delete-topic <名称> - 删除Topic"
    echo "  producer [topic]   - 启动测试生产者"
    echo "  consumer [topic]   - 启动测试消费者"
    echo "  help               - 显示帮助"
    echo ""
    echo "示例:"
    echo "  $0 start"
    echo "  $0 status"
    echo "  $0 create-topic my-new-topic"
    echo "  $0 producer my-topic"
    echo "  $0 logs kafka"
}

# 主命令处理
case "${1:-help}" in
    start)
        start_zookeeper
        start_kafka
        status
        ;;
    stop)
        stop_kafka
        stop_zookeeper
        ;;
    restart)
        stop_kafka
        stop_zookeeper
        sleep 2
        start_zookeeper
        start_kafka
        status
        ;;
    status)
        status
        ;;
    logs)
        logs "$2"
        ;;
    list-topics|list)
        list_topics
        ;;
    create-topic|create)
        create_topic "$2"
        ;;
    delete-topic|delete)
        delete_topic "$2"
        ;;
    producer|produce)
        test_producer "$2"
        ;;
    consumer|consume)
        test_consumer "$2"
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        echo -e "${RED}错误: 未知命令 '$1'${NC}"
        echo ""
        show_help
        exit 1
        ;;
esac