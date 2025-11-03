#!/bin/bash

# 聊天室监控系统停止脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo ""
echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  聊天室监控系统停止脚本${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""

# 检测 Docker Compose 命令
if command -v docker-compose &> /dev/null; then
    COMPOSE_CMD="docker-compose"
elif docker compose version &> /dev/null; then
    COMPOSE_CMD="docker compose"
else
    echo -e "${RED}❌ Docker Compose 未安装${NC}"
    exit 1
fi

# 显示当前运行的服务
echo -e "${BLUE}📊 当前运行的监控服务:${NC}"
$COMPOSE_CMD ps
echo ""

# 询问用户确认
read -p "是否停止所有监控服务? (y/n) " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}已取消操作${NC}"
    exit 0
fi

echo ""
echo -e "${YELLOW}🛑 正在停止监控服务...${NC}"
$COMPOSE_CMD down

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  监控系统已停止！${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""
echo -e "${BLUE}提示:${NC}"
echo "  - 容器已停止并删除"
echo "  - 数据卷已保留（Prometheus、Grafana 数据）"
echo "  - 网络已删除"
echo ""
echo -e "${BLUE}如需删除所有数据（包括 Grafana Dashboard 和 Prometheus 历史数据）:${NC}"
echo "  $COMPOSE_CMD down -v"
echo ""
echo -e "${BLUE}重新启动监控系统:${NC}"
echo "  ./start.sh"
echo ""
