#!/bin/bash

echo "聊天室客户端使用说明"
echo "===================="
echo
echo "1. 编译项目："
echo "   cd /home/yang/chatroom/server"
echo "   mkdir build && cd build"
echo "   cmake .."
echo "   make"
echo
echo "2. 启动服务器："
echo "   ./application/chat-room/chat-room"
echo
echo "3. 启动客户端（在新终端中）："
echo "   ./application/client/chat-client [服务器IP] [端口]"
echo
echo "4. 默认连接参数："
echo "   服务器IP: 127.0.0.1"
echo "   端口: 8080"
echo
echo "5. 客户端命令："
echo "   - 输入任意文本发送消息"
echo "   - 输入 'quit' 或 'exit' 退出客户端"
echo
echo "示例："
echo "   ./application/client/chat-client                    # 连接到本地服务器"
echo "   ./application/client/chat-client 192.168.1.100     # 连接到指定IP"
echo "   ./application/client/chat-client 192.168.1.100 9090 # 连接到指定IP和端口"