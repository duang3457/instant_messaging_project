#!/bin/bash

# 聊天室服务状态检查脚本

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 辅助函数：检查端口
check_port() {
    local port=$1
    if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
        echo -e "${GREEN}✓${NC}"
        return 0
    else
        echo -e "${RED}✗${NC}"
        return 1
    fi
}

# 辅助函数：检查进程
check_process() {
    local name=$1
    if pgrep -f "$name" > /dev/null 2>&1; then
        local pid=$(pgrep -f "$name" | head -1)
        echo -e "${GREEN}✓${NC} (PID: $pid)"
        return 0
    else
        echo -e "${RED}✗${NC}"
        return 1
    fi
}

# 辅助函数：检查 HTTP 端点
check_http() {
    local url=$1
    if curl -s -f -o /dev/null "$url" 2>/dev/null; then
        echo -e "${GREEN}✓${NC}"
        return 0
    else
        echo -e "${RED}✗${NC}"
        return 1
    fi
}

echo ""
echo -e "${CYAN}================================================${NC}"
echo -e "${CYAN}        聊天室服务状态检查${NC}"
echo -e "${CYAN}================================================${NC}"
echo ""

# ==================== 基础服务 ====================
echo -e "${BLUE}【基础服务】${NC}"
echo ""

# Zookeeper
echo -n "  Zookeeper (2181)           : "
check_port 2181
zk_status=$?

# Kafka
echo -n "  Kafka (9092)               : "
check_port 9092
kafka_status=$?

echo ""

# ==================== 应用服务 ====================
echo -e "${BLUE}【应用服务】${NC}"
echo ""

# Comet
echo -n "  Comet - WebSocket (8082)   : "
check_port 8082
comet_ws_status=$?

echo -n "  Comet - gRPC (50051)       : "
check_port 50051
comet_grpc_status=$?

echo -n "  Comet - Metrics (9091)     : "
check_port 9091
comet_metrics_status=$?

echo -n "  Comet - 进程状态           : "
check_process "application/chat-room/chat-room"
comet_proc_status=$?

# Logic
echo -n "  Logic - HTTP (8090)        : "
check_port 8090
logic_status=$?

echo -n "  Logic - 进程状态           : "
check_process "application/logic/logic"
logic_proc_status=$?

# Job
echo -n "  Job - 进程状态             : "
check_process "application/job/job"
job_status=$?

echo ""

# ==================== 监控服务 ====================
echo -e "${BLUE}【监控服务】${NC}"
echo ""

echo -n "  Prometheus (9090)          : "
check_port 9090
prom_status=$?

echo -n "  Grafana (3000)             : "
check_port 3000
grafana_status=$?

echo ""

# ==================== 健康检查 ====================
echo -e "${BLUE}【健康检查】${NC}"
echo ""

# Comet Metrics
if [ $comet_metrics_status -eq 0 ]; then
    echo -n "  Comet Metrics 端点         : "
    check_http "http://localhost:9091/metrics"
fi

# Prometheus Targets
if [ $prom_status -eq 0 ]; then
    echo -n "  Prometheus 健康检查        : "
    check_http "http://localhost:9090/-/healthy"
fi

# Grafana API
if [ $grafana_status -eq 0 ]; then
    echo -n "  Grafana API 健康检查       : "
    check_http "http://localhost:3000/api/health"
fi

echo ""

# ==================== 总结 ====================
echo -e "${CYAN}================================================${NC}"
echo -e "${BLUE}【服务总结】${NC}"
echo ""

total=0
running=0

# 统计基础服务
total=$((total + 2))
[ $zk_status -eq 0 ] && running=$((running + 1))
[ $kafka_status -eq 0 ] && running=$((running + 1))

# 统计应用服务
total=$((total + 3))
[ $comet_proc_status -eq 0 ] && running=$((running + 1))
[ $logic_proc_status -eq 0 ] && running=$((running + 1))
[ $job_status -eq 0 ] && running=$((running + 1))

echo -e "  运行中服务: ${GREEN}$running${NC} / $total"
echo ""

if [ $running -eq $total ]; then
    echo -e "  状态: ${GREEN}所有核心服务正常运行 ✓${NC}"
    exit_code=0
elif [ $running -eq 0 ]; then
    echo -e "  状态: ${RED}所有服务均未运行 ✗${NC}"
    echo ""
    echo -e "  ${YELLOW}提示: 运行 ./start_all.sh 启动所有服务${NC}"
    exit_code=1
else
    echo -e "  状态: ${YELLOW}部分服务未运行 ⚠${NC}"
    echo ""
    echo -e "  ${YELLOW}未运行的服务:${NC}"
    [ $zk_status -ne 0 ] && echo "    - Zookeeper"
    [ $kafka_status -ne 0 ] && echo "    - Kafka"
    [ $comet_proc_status -ne 0 ] && echo "    - Comet"
    [ $logic_proc_status -ne 0 ] && echo "    - Logic"
    [ $job_status -ne 0 ] && echo "    - Job"
    exit_code=2
fi

echo ""

# ==================== 快捷命令 ====================
echo -e "${BLUE}【快捷命令】${NC}"
echo ""
echo "  # 启动所有服务"
echo "  ./start_all.sh"
echo ""
echo "  # 停止所有服务"
echo "  ./stop_all.sh"
echo ""
echo "  # 查看实时日志"
echo "  tail -f ~/chatroom/logs/comet.log"
echo "  tail -f ~/chatroom/logs/logic.log"
echo "  tail -f ~/chatroom/logs/job.log"
echo ""
echo "  # 查看进程"
echo "  ps aux | grep -E 'zookeeper|kafka|chat-room|logic|job' | grep -v grep"
echo ""
echo "  # 查看端口监听"
echo "  netstat -tlnp | grep -E '2181|9092|8082|8090|50051|9091'"
echo ""
echo -e "${CYAN}================================================${NC}"
echo ""

exit $exit_code
