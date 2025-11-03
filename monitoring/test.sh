#!/bin/bash

# 监控系统测试脚本

set -e

echo "======================================"
echo "  监控系统功能测试"
echo "======================================"
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 测试函数
test_endpoint() {
    local name=$1
    local url=$2
    local expected=$3
    
    echo -n "测试 $name ... "
    
    if curl -s -f "$url" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ 成功${NC}"
        return 0
    else
        echo -e "${RED}✗ 失败${NC}"
        return 1
    fi
}

test_metrics() {
    local name=$1
    local url=$2
    local metric=$3
    
    echo -n "测试 $name 指标 '$metric' ... "
    
    response=$(curl -s "$url")
    if echo "$response" | grep -q "$metric"; then
        echo -e "${GREEN}✓ 存在${NC}"
        return 0
    else
        echo -e "${RED}✗ 不存在${NC}"
        return 1
    fi
}

echo "1️⃣  测试监控基础设施"
echo "-----------------------------------"
test_endpoint "Prometheus UI" "http://localhost:9090/-/healthy"
test_endpoint "Grafana UI" "http://localhost:3000/api/health"
test_endpoint "Alertmanager UI" "http://localhost:9093/-/healthy"
test_endpoint "Loki API" "http://localhost:3100/ready"
echo ""

echo "2️⃣  测试 Prometheus Targets"
echo "-----------------------------------"
echo "访问 http://localhost:9090/targets 查看抓取目标状态"
echo ""

echo "3️⃣  测试 Comet Metrics 端点"
echo "-----------------------------------"
if test_endpoint "Comet /metrics" "http://localhost:9091/metrics"; then
    echo ""
    echo "Comet 关键指标检查:"
    test_metrics "Comet" "http://localhost:9091/metrics" "http_requests_total"
    test_metrics "Comet" "http://localhost:9091/metrics" "active_connections"
    test_metrics "Comet" "http://localhost:9091/metrics" "http_request_duration_microseconds"
    test_metrics "Comet" "http://localhost:9091/metrics" "errors_total"
    test_metrics "Comet" "http://localhost:9091/metrics" "websocket_messages_pushed_total"
    test_metrics "Comet" "http://localhost:9091/metrics" "redis_operations_total"
else
    echo -e "${YELLOW}⚠️  Comet 服务未启动，请先启动: cd /home/yang/chatroom/server/build && ./chat-room${NC}"
fi
echo ""

echo "4️⃣  测试 PromQL 查询"
echo "-----------------------------------"
echo "查询 Comet 服务状态:"
response=$(curl -s "http://localhost:9090/api/v1/query?query=up%7Bservice%3D%22comet%22%7D" | jq -r '.data.result[0].value[1]')
if [ "$response" == "1" ]; then
    echo -e "${GREEN}✓ Comet 服务在线${NC}"
else
    echo -e "${RED}✗ Comet 服务离线${NC}"
fi
echo ""

echo "5️⃣  测试 Grafana 数据源"
echo "-----------------------------------"
echo "登录 Grafana 测试数据源:"
echo "  URL: http://localhost:3000"
echo "  用户名: admin"
echo "  密码: admin123"
echo ""
echo "导航到: Configuration → Data Sources"
echo "应该看到 Prometheus 和 Loki 两个数据源"
echo ""

echo "6️⃣  生成测试数据"
echo "-----------------------------------"
echo "发送测试请求到 Comet..."

# 检查 Comet 是否在运行
if curl -s -f "http://localhost:9091/metrics" > /dev/null 2>&1; then
    echo "模拟 10 个 HTTP 请求..."
    for i in {1..10}; do
        curl -s "http://localhost:8081/" > /dev/null 2>&1 || true
        sleep 0.1
    done
    echo -e "${GREEN}✓ 已发送测试请求${NC}"
    
    echo ""
    echo "查看 QPS 变化:"
    echo "  访问: http://localhost:3000"
    echo "  Dashboard: 聊天室系统监控 - Overview"
    echo "  或使用 Explore 查询: rate(http_requests_total[1m])"
else
    echo -e "${YELLOW}⚠️  Comet 未运行，跳过测试数据生成${NC}"
fi
echo ""

echo "7️⃣  测试告警规则"
echo "-----------------------------------"
echo "查看告警规则:"
curl -s "http://localhost:9090/api/v1/rules" | jq -r '.data.groups[].rules[].name' | head -5
echo ""

echo "======================================"
echo "  测试总结"
echo "======================================"
echo ""
echo "✅ 监控系统已部署"
echo ""
echo "下一步操作:"
echo "  1. 访问 Grafana: http://localhost:3000 (admin/admin123)"
echo "  2. 查看 Dashboard: 聊天室系统监控 - Overview"
echo "  3. 使用 Explore 查询实时指标"
echo "  4. 配置钉钉告警（编辑 alertmanager.yml）"
echo ""
echo "监控端点:"
echo "  • Comet Metrics:  http://localhost:9091/metrics"
echo "  • Prometheus:     http://localhost:9090"
echo "  • Grafana:        http://localhost:3000"
echo "  • Alertmanager:   http://localhost:9093"
echo ""
