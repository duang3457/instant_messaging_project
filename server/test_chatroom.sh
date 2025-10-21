#!/bin/bash

echo "聊天室测试脚本"
echo "=============="

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../../build"

# 检查是否已编译
if [ ! -f "$BUILD_DIR/application/chat-room/chat-room" ]; then
    echo "错误: 请先编译项目"
    echo "运行: cd $SCRIPT_DIR/../.. && mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

if [ ! -f "$BUILD_DIR/application/client/chat-client" ]; then
    echo "错误: 客户端未编译"
    exit 1
fi

echo "启动服务器..."
cd "$BUILD_DIR"

# 启动服务器（后台运行）
./application/chat-room/chat-room &
SERVER_PID=$!

echo "服务器已启动 (PID: $SERVER_PID)"
echo "等待2秒..."
sleep 2

echo ""
echo "现在您可以启动客户端进行测试："
echo "cd $BUILD_DIR"
echo "./application/client/chat-client"
echo ""
echo "或者在另一个终端中运行上述命令"
echo ""
echo "要停止服务器，请运行: kill $SERVER_PID"