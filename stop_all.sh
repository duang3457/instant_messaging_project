#!/bin/bash

# 聊天室服务停止脚本
# 按依赖顺序反向停止：Job → Logic → Comet → Kafka → Zookeeper

set -e  # 遇到错误立即退出

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置
KAFKA_HOME="$HOME/kafka"

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

# 辅助函数：停止进程
kill_process() {
    local name=$1
    local signal=${2:-TERM}  # 默认使用 SIGTERM
    
    local pids=$(pgrep -f "$name" 2>/dev/null)
    if [ -z "$pids" ]; then
        print_warning "$name 未运行"
        return 0
    fi
    
    print_info "停止 $name (PID: $pids)..."
    
    if [ "$signal" == "TERM" ]; then
        kill $pids 2>/dev/null || true
    else
        kill -9 $pids 2>/dev/null || true
    fi
    
    # 等待进程结束
    local count=0
    while [ $count -lt 10 ]; do
        if ! pgrep -f "$name" > /dev/null 2>&1; then
            print_success "$name 已停止"
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    
    # 如果还没停止，强制杀死
    if pgrep -f "$name" > /dev/null 2>&1; then
        print_warning "$name 未响应，强制停止..."
        pkill -9 -f "$name" 2>/dev/null || true
        sleep 1
        if pgrep -f "$name" > /dev/null 2>&1; then
            print_error "$name 停止失败"
            return 1
        else
            print_success "$name 已强制停止"
        fi
    fi
    
    return 0
}

print_info "========================================"
print_info "停止所有聊天室服务"
print_info "========================================"
echo ""

# ==================== 1. 停止 Job ====================
print_info "步骤 1/5: 停止 Job (Kafka 消费者)"
kill_process "application/job/job"
echo ""

# ==================== 2. 停止 Logic ====================
print_info "步骤 2/5: 停止 Logic (业务逻辑层)"
kill_process "application/logic/logic"
echo ""

# ==================== 3. 停止 Comet ====================
print_info "步骤 3/5: 停止 Comet (WebSocket 接入层)"
kill_process "application/chat-room/chat-room"
echo ""

# ==================== 4. 停止 Kafka ====================
print_info "步骤 4/5: 停止 Kafka"
if [ -f "$KAFKA_HOME/bin/kafka-server-stop.sh" ]; then
    "$KAFKA_HOME/bin/kafka-server-stop.sh" 2>/dev/null || true
    sleep 3
fi
kill_process "kafka.Kafka"
echo ""

# ==================== 5. 停止 Zookeeper ====================
print_info "步骤 5/5: 停止 Zookeeper"
if [ -f "$KAFKA_HOME/bin/zookeeper-server-stop.sh" ]; then
    "$KAFKA_HOME/bin/zookeeper-server-stop.sh" 2>/dev/null || true
    sleep 3
fi
kill_process "org.apache.zookeeper.server.quorum.QuorumPeerMain"
echo ""

# ==================== 验证 ====================
print_info "========================================"
print_info "验证服务状态"
print_info "========================================"
echo ""

all_stopped=true

# 检查 Job
if pgrep -f "application/job/job" > /dev/null 2>&1; then
    print_error "Job 仍在运行"
    all_stopped=false
else
    print_success "Job 已停止"
fi

# 检查 Logic
if pgrep -f "application/logic/logic" > /dev/null 2>&1; then
    print_error "Logic 仍在运行"
    all_stopped=false
else
    print_success "Logic 已停止"
fi

# 检查 Comet
if pgrep -f "application/chat-room/chat-room" > /dev/null 2>&1; then
    print_error "Comet 仍在运行"
    all_stopped=false
else
    print_success "Comet 已停止"
fi

# 检查 Kafka
if pgrep -f "kafka.Kafka" > /dev/null 2>&1; then
    print_error "Kafka 仍在运行"
    all_stopped=false
else
    print_success "Kafka 已停止"
fi

# 检查 Zookeeper
if pgrep -f "org.apache.zookeeper.server.quorum.QuorumPeerMain" > /dev/null 2>&1; then
    print_error "Zookeeper 仍在运行"
    all_stopped=false
else
    print_success "Zookeeper 已停止"
fi

echo ""

# ==================== 6. 停止监控服务（可选）====================
echo ""
read -p "是否同时停止监控服务 (Prometheus/Grafana)? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    print_info "步骤 6/6: 停止监控服务"
    MONITORING_DIR="$HOME/chatroom/monitoring"
    if [ -f "$MONITORING_DIR/stop.sh" ]; then
        cd "$MONITORING_DIR"
        # 直接调用 docker-compose down，不需要交互确认
        if command -v docker-compose &> /dev/null; then
            COMPOSE_CMD="docker-compose"
        elif docker compose version &> /dev/null 2>&1; then
            COMPOSE_CMD="docker compose"
        else
            print_warning "Docker Compose 未找到，跳过监控服务停止"
        fi
        
        if [ ! -z "$COMPOSE_CMD" ]; then
            print_info "停止监控容器..."
            $COMPOSE_CMD down 2>/dev/null || true
            print_success "监控服务已停止"
        fi
    else
        print_warning "未找到监控停止脚本，跳过"
    fi
    echo ""
fi

if $all_stopped; then
    print_success "所有应用服务已成功停止！"
    exit 0
else
    print_error "部分服务停止失败，请手动检查"
    echo ""
    echo -e "${YELLOW}提示：可以使用以下命令强制停止：${NC}"
    echo "  pkill -9 -f 'application/job/job'"
    echo "  pkill -9 -f 'application/logic/logic'"
    echo "  pkill -9 -f 'application/chat-room/chat-room'"
    echo "  pkill -9 -f 'kafka.Kafka'"
    echo "  pkill -9 -f 'QuorumPeerMain'"
    exit 1
fi
