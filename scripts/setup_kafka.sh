#!/bin/bash

# Kafka安装和启动脚本
# 适用于Ubuntu/Debian系统

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Kafka配置
KAFKA_VERSION="3.8.0"
SCALA_VERSION="2.13"
KAFKA_DIR="$HOME/kafka"
KAFKA_PACKAGE="kafka_${SCALA_VERSION}-${KAFKA_VERSION}"
DOWNLOAD_URL="https://archive.apache.org/dist/kafka/${KAFKA_VERSION}/${KAFKA_PACKAGE}.tgz"

echo -e "${GREEN}=== Kafka安装和启动脚本 ===${NC}"

# 检查Java是否已安装
check_java() {
    echo -e "${BLUE}检查Java环境...${NC}"
    if command -v java >/dev/null 2>&1; then
        JAVA_VERSION=$(java -version 2>&1 | head -n 1 | cut -d'"' -f2)
        echo -e "${GREEN}✓ Java已安装: $JAVA_VERSION${NC}"
        return 0
    else
        echo -e "${YELLOW}Java未安装，正在安装...${NC}"
        return 1
    fi
}

# 安装Java
install_java() {
    echo -e "${BLUE}安装OpenJDK 11...${NC}"
    sudo apt-get update
    sudo apt-get install -y openjdk-11-jdk
    
    if command -v java >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Java安装成功${NC}"
    else
        echo -e "${RED}✗ Java安装失败${NC}"
        exit 1
    fi
}

# 下载和安装Kafka
install_kafka() {
    echo -e "${BLUE}下载Kafka ${KAFKA_VERSION}...${NC}"
    
    cd ~
    
    # 如果已存在，先备份
    if [[ -d "$KAFKA_DIR" ]]; then
        echo -e "${YELLOW}发现已存在的Kafka目录，重命名为 kafka.bak...${NC}"
        mv "$KAFKA_DIR" "${KAFKA_DIR}.bak.$(date +%Y%m%d_%H%M%S)"
    fi
    
    # 下载Kafka
    if [[ ! -f "${KAFKA_PACKAGE}.tgz" ]]; then
        echo "正在下载 ${DOWNLOAD_URL}..."
        wget "$DOWNLOAD_URL" || {
            echo -e "${RED}下载失败，请检查网络连接${NC}"
            exit 1
        }
    else
        echo -e "${YELLOW}发现已下载的Kafka包${NC}"
    fi
    
    # 解压
    echo -e "${BLUE}解压Kafka...${NC}"
    tar -xzf "${KAFKA_PACKAGE}.tgz"
    mv "${KAFKA_PACKAGE}" kafka
    
    echo -e "${GREEN}✓ Kafka安装完成: $KAFKA_DIR${NC}"
}

# 配置Kafka
configure_kafka() {
    echo -e "${BLUE}配置Kafka...${NC}"
    
    # 创建日志目录
    mkdir -p "$KAFKA_DIR/logs"
    mkdir -p "$KAFKA_DIR/data/zookeeper"
    mkdir -p "$KAFKA_DIR/data/kafka"
    
    # 修改Zookeeper配置
    cat > "$KAFKA_DIR/config/zookeeper.properties" << EOF
# Zookeeper配置
dataDir=$KAFKA_DIR/data/zookeeper
clientPort=2181
maxClientCnxns=0
admin.enableServer=false
tickTime=2000
initLimit=5
syncLimit=2
EOF
    
    # 修改Kafka配置
    sed -i "s|log.dirs=.*|log.dirs=$KAFKA_DIR/data/kafka|g" "$KAFKA_DIR/config/server.properties"
    
    echo -e "${GREEN}✓ Kafka配置完成${NC}"
}

# 启动Zookeeper
start_zookeeper() {
    echo -e "${BLUE}启动Zookeeper...${NC}"
    
    # 检查是否已运行
    if lsof -i :2181 >/dev/null 2>&1; then
        echo -e "${YELLOW}Zookeeper已在运行${NC}"
        return 0
    fi
    
    # 启动Zookeeper
    cd "$KAFKA_DIR"
    nohup bin/zookeeper-server-start.sh config/zookeeper.properties > logs/zookeeper.log 2>&1 &
    
    echo "等待Zookeeper启动..."
    sleep 5
    
    if lsof -i :2181 >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Zookeeper启动成功 (端口2181)${NC}"
    else
        echo -e "${RED}✗ Zookeeper启动失败，查看日志: $KAFKA_DIR/logs/zookeeper.log${NC}"
        exit 1
    fi
}

# 启动Kafka
start_kafka() {
    echo -e "${BLUE}启动Kafka...${NC}"
    
    # 检查是否已运行
    if lsof -i :9092 >/dev/null 2>&1; then
        echo -e "${YELLOW}Kafka已在运行${NC}"
        return 0
    fi
    
    # 启动Kafka
    cd "$KAFKA_DIR"
    nohup bin/kafka-server-start.sh config/server.properties > logs/kafka.log 2>&1 &
    
    echo "等待Kafka启动..."
    sleep 10
    
    if lsof -i :9092 >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Kafka启动成功 (端口9092)${NC}"
    else
        echo -e "${RED}✗ Kafka启动失败，查看日志: $KAFKA_DIR/logs/kafka.log${NC}"
        exit 1
    fi
}

# 创建测试Topic
create_topic() {
    echo -e "${BLUE}创建测试Topic...${NC}"
    
    cd "$KAFKA_DIR"
    
    # 检查Topic是否已存在
    if bin/kafka-topics.sh --list --bootstrap-server localhost:9092 | grep -q "my-topic"; then
        echo -e "${YELLOW}Topic 'my-topic' 已存在${NC}"
    else
        bin/kafka-topics.sh --create \
            --bootstrap-server localhost:9092 \
            --topic my-topic \
            --partitions 3 \
            --replication-factor 1
        
        echo -e "${GREEN}✓ Topic 'my-topic' 创建成功${NC}"
    fi
    
    # 列出所有Topic
    echo -e "${BLUE}当前Topic列表:${NC}"
    bin/kafka-topics.sh --list --bootstrap-server localhost:9092
}

# 显示状态
show_status() {
    echo ""
    echo -e "${GREEN}=== Kafka服务状态 ===${NC}"
    echo -e "${BLUE}Kafka目录:${NC} $KAFKA_DIR"
    
    if lsof -i :2181 >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Zookeeper运行中 (端口2181)${NC}"
    else
        echo -e "${RED}✗ Zookeeper未运行${NC}"
    fi
    
    if lsof -i :9092 >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Kafka运行中 (端口9092)${NC}"
    else
        echo -e "${RED}✗ Kafka未运行${NC}"
    fi
    
    echo ""
    echo -e "${BLUE}常用命令:${NC}"
    echo "  查看日志: tail -f $KAFKA_DIR/logs/kafka.log"
    echo "  停止Kafka: $KAFKA_DIR/bin/kafka-server-stop.sh"
    echo "  停止Zookeeper: $KAFKA_DIR/bin/zookeeper-server-stop.sh"
    echo "  创建Topic: $KAFKA_DIR/bin/kafka-topics.sh --create --bootstrap-server localhost:9092 --topic <topic-name>"
    echo "  查看Topic: $KAFKA_DIR/bin/kafka-topics.sh --list --bootstrap-server localhost:9092"
    echo "  测试生产: $KAFKA_DIR/bin/kafka-console-producer.sh --bootstrap-server localhost:9092 --topic my-topic"
    echo "  测试消费: $KAFKA_DIR/bin/kafka-console-consumer.sh --bootstrap-server localhost:9092 --topic my-topic --from-beginning"
}

# 主流程
main() {
    # 检查并安装Java
    if ! check_java; then
        install_java
    fi
    
    # 检查Kafka是否已安装
    if [[ ! -d "$KAFKA_DIR" ]]; then
        echo -e "${YELLOW}Kafka未安装，开始安装...${NC}"
        install_kafka
        configure_kafka
    else
        echo -e "${GREEN}✓ Kafka已安装: $KAFKA_DIR${NC}"
    fi
    
    # 启动服务
    start_zookeeper
    start_kafka
    
    # 创建测试Topic
    create_topic
    
    # 显示状态
    show_status
    
    echo ""
    echo -e "${GREEN}Kafka服务启动完成！${NC}"
}

# 运行主流程
main