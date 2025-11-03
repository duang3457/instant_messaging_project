#!/bin/bash

# 聊天室服务启动脚本
# 按依赖顺序依次启动：Zookeeper → Kafka → Comet → Logic → Job

set -e  # 遇到错误立即退出

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置
KAFKA_HOME="$HOME/kafka"
SERVER_BUILD_DIR="$HOME/chatroom/server/build"
LOG_DIR="$HOME/chatroom/logs"

# 创建日志目录
mkdir -p "$LOG_DIR"

# 辅助函数：打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 辅助函数：检查端口是否被占用
check_port() {
    local port=$1
    if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
        return 0  # 端口被占用
    else
        return 1  # 端口空闲
    fi
}

# 辅助函数：等待端口可用
wait_for_port() {
    local port=$1
    local service=$2
    local max_wait=${3:-30}  # 默认等待30秒
    local count=0
    
    print_info "等待 $service 启动（端口 $port）..."
    while [ $count -lt $max_wait ]; do
        if check_port $port; then
            print_success "$service 已启动！"
            return 0
        fi
        sleep 1
        count=$((count + 1))
        echo -n "."
    done
    echo ""
    print_error "$service 启动超时（端口 $port 未监听）"
    return 1
}

# 辅助函数：检查进程是否运行
check_process() {
    local name=$1
    if pgrep -f "$name" > /dev/null; then
        return 0  # 进程运行中
    else
        return 1  # 进程未运行
    fi
}

# ==================== 1. 启动 Zookeeper ====================
print_info "========================================"
print_info "步骤 1/5: 启动 Zookeeper"
print_info "========================================"

if check_port 2181; then
    print_warning "Zookeeper 已在运行（端口 2181）"
else
    if [ ! -f "$KAFKA_HOME/bin/zookeeper-server-start.sh" ]; then
        print_error "未找到 Zookeeper 启动脚本：$KAFKA_HOME/bin/zookeeper-server-start.sh"
        exit 1
    fi
    
    print_info "启动 Zookeeper..."
    nohup "$KAFKA_HOME/bin/zookeeper-server-start.sh" \
        "$KAFKA_HOME/config/zookeeper.properties" \
        > "$LOG_DIR/zookeeper.log" 2>&1 &
    
    if ! wait_for_port 2181 "Zookeeper" 30; then
        print_error "Zookeeper 启动失败，请检查日志：$LOG_DIR/zookeeper.log"
        exit 1
    fi
fi

# ==================== 2. 启动 Kafka ====================
print_info ""
print_info "========================================"
print_info "步骤 2/5: 启动 Kafka"
print_info "========================================"

if check_port 9092; then
    print_warning "Kafka 已在运行（端口 9092）"
else
    if [ ! -f "$KAFKA_HOME/bin/kafka-server-start.sh" ]; then
        print_error "未找到 Kafka 启动脚本：$KAFKA_HOME/bin/kafka-server-start.sh"
        exit 1
    fi
    
    print_info "启动 Kafka..."
    nohup "$KAFKA_HOME/bin/kafka-server-start.sh" \
        "$KAFKA_HOME/config/server.properties" \
        > "$LOG_DIR/kafka.log" 2>&1 &
    
    if ! wait_for_port 9092 "Kafka" 60; then
        print_error "Kafka 启动失败，请检查日志：$LOG_DIR/kafka.log"
        exit 1
    fi
    
    # 等待 Kafka 完全就绪
    print_info "等待 Kafka 完全就绪..."
    sleep 5
fi

# ==================== 3. 启动 Comet (chat-room) ====================
print_info ""
print_info "========================================"
print_info "步骤 3/5: 启动 Comet (WebSocket 接入层)"
print_info "========================================"

if check_port 8082; then
    print_warning "Comet 已在运行（端口 8082）"
else
    if [ ! -f "$SERVER_BUILD_DIR/application/chat-room/chat-room" ]; then
        print_error "未找到 Comet 可执行文件：$SERVER_BUILD_DIR/application/chat-room/chat-room"
        print_info "请先编译项目：cd $SERVER_BUILD_DIR && make"
        exit 1
    fi
    
    print_info "启动 Comet..."
    cd "$SERVER_BUILD_DIR"
    nohup ./application/chat-room/chat-room \
        > "$LOG_DIR/comet.log" 2>&1 &
    
    if ! wait_for_port 8082 "Comet (HTTP/WebSocket)" 30; then
        print_error "Comet 启动失败，请检查日志：$LOG_DIR/comet.log"
        exit 1
    fi
    
    # 检查 gRPC 端口
    if ! wait_for_port 50051 "Comet (gRPC)" 10; then
        print_warning "Comet gRPC 端口 50051 未启动"
    fi
    
    # 检查 Metrics 端口
    if ! wait_for_port 9091 "Comet (Metrics)" 10; then
        print_warning "Comet Metrics 端口 9091 未启动"
    fi
fi

# ==================== 4. 启动 Logic ====================
print_info ""
print_info "========================================"
print_info "步骤 4/5: 启动 Logic (业务逻辑层)"
print_info "========================================"

if check_port 8090; then
    print_warning "Logic 已在运行（端口 8090）"
else
    if [ ! -f "$SERVER_BUILD_DIR/application/logic/logic" ]; then
        print_error "未找到 Logic 可执行文件：$SERVER_BUILD_DIR/application/logic/logic"
        print_info "请先编译项目：cd $SERVER_BUILD_DIR && make"
        exit 1
    fi
    
    print_info "启动 Logic..."
    cd "$SERVER_BUILD_DIR"
    nohup ./application/logic/logic \
        > "$LOG_DIR/logic.log" 2>&1 &
    
    if ! wait_for_port 8090 "Logic" 30; then
        print_error "Logic 启动失败，请检查日志：$LOG_DIR/logic.log"
        exit 1
    fi
fi

# ==================== 5. 启动 Job ====================
print_info ""
print_info "========================================"
print_info "步骤 5/5: 启动 Job (Kafka 消费者)"
print_info "========================================"

if check_process "application/job/job"; then
    print_warning "Job 已在运行"
else
    if [ ! -f "$SERVER_BUILD_DIR/application/job/job" ]; then
        print_error "未找到 Job 可执行文件：$SERVER_BUILD_DIR/application/job/job"
        print_info "请先编译项目：cd $SERVER_BUILD_DIR && make"
        exit 1
    fi
    
    print_info "启动 Job..."
    cd "$SERVER_BUILD_DIR"
    nohup ./application/job/job \
        > "$LOG_DIR/job.log" 2>&1 &
    
    # Job 没有监听端口，检查进程
    sleep 3
    if check_process "application/job/job"; then
        print_success "Job 已启动！"
    else
        print_error "Job 启动失败，请检查日志：$LOG_DIR/job.log"
        exit 1
    fi
fi

# ==================== 总结 ====================
print_info ""
print_info "========================================"
print_info "所有服务启动完成！"
print_info "========================================"
echo ""
echo -e "${GREEN}服务状态：${NC}"
echo "  ✓ Zookeeper      - 端口 2181"
echo "  ✓ Kafka          - 端口 9092"
echo "  ✓ Comet          - HTTP/WS: 8082, gRPC: 50051, Metrics: 9091"
echo "  ✓ Logic          - 端口 8090"
echo "  ✓ Job            - Kafka Consumer (无端口)"
echo ""
echo -e "${BLUE}日志文件位置：${NC}"
echo "  - Zookeeper: $LOG_DIR/zookeeper.log"
echo "  - Kafka:     $LOG_DIR/kafka.log"
echo "  - Comet:     $LOG_DIR/comet.log"
echo "  - Logic:     $LOG_DIR/logic.log"
echo "  - Job:       $LOG_DIR/job.log"
echo ""
echo -e "${BLUE}快速检查：${NC}"
echo "  # 查看所有服务进程"
echo "  ps aux | grep -E 'zookeeper|kafka|chat-room|logic|job' | grep -v grep"
echo ""
echo "  # 查看端口监听"
echo "  netstat -tlnp | grep -E '2181|9092|8082|8090|50051|9091'"
echo ""
echo "  # 实时查看日志"
echo "  tail -f $LOG_DIR/comet.log"
echo ""
echo -e "${BLUE}访问地址：${NC}"
echo "  - WebSocket: ws://localhost:8082"
echo "  - Logic API: http://localhost:8090"
echo "  - Prometheus: http://localhost:9091/metrics"
echo "  - Grafana: http://localhost:3000 (如果已启动监控)"
echo ""
echo -e "${GREEN}启动完成！${NC}"
