#!/bin/bash

# 聊天室监控系统启动脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================"
echo "  聊天室监控系统启动脚本"
echo "======================================"
echo ""

# 检查 Docker 是否安装
if ! command -v docker &> /dev/null; then
    echo "❌ Docker 未安装，请先安装 Docker"
    exit 1
fi

# 检查 Docker Compose 是否安装
if command -v docker-compose &> /dev/null; then
    COMPOSE_CMD="docker-compose"
    echo "检测到 docker-compose (standalone)"
elif docker compose version &> /dev/null; then
    COMPOSE_CMD="docker compose"
    echo "检测到 docker compose (plugin)"
else
    echo "❌ Docker Compose 未安装"
    echo ""
    echo "请选择以下方式之一安装 Docker Compose："
    echo ""
    echo "方式 1: 使用 apt 安装 (推荐)"
    echo "  sudo apt update"
    echo "  sudo apt install docker-compose"
    echo ""
    echo "方式 2: 使用 Docker Compose Plugin"
    echo "  sudo apt install docker-compose-plugin"
    echo ""
    echo "安装后重新运行此脚本"
    exit 1
fi

echo "✅ Docker 和 Docker Compose 已安装"
echo ""

# 启动监控服务
echo "🚀 启动监控服务..."
$COMPOSE_CMD up -d

echo ""
echo "⏳ 等待服务启动..."
sleep 10

# 检查服务状态
echo ""
echo "📊 服务状态:"
$COMPOSE_CMD ps

echo ""
echo "======================================"
echo "  监控系统启动成功！"
echo "======================================"
echo ""
echo "访问地址:"
echo "  - Grafana:       http://localhost:3000"
echo "    用户名: admin"
echo "    密码:   admin123"
echo ""
echo "  - Prometheus:    http://localhost:9090"
echo "  - Alertmanager:  http://localhost:9093"
echo ""
echo "下一步:"
echo "  1. 启动 Comet 服务: cd ../server/build && ./chat-room"
echo "  2. 访问 Grafana 创建 Dashboard"
echo "  3. 查看 README.md 了解详细配置"
echo ""
echo "查看日志:"
echo "  $COMPOSE_CMD logs -f [service_name]"
echo ""
echo "停止服务:"
echo "  $COMPOSE_CMD down"
echo ""
