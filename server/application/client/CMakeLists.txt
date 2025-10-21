# chat-client 项目配置
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

# 系统库包含目录
INCLUDE_DIRECTORIES(/usr/local/include)

# 创建客户端可执行文件
ADD_EXECUTABLE(chat-client chat_client.cc)

# 链接库
TARGET_LINK_LIBRARIES(chat-client 
    muduo_net 
    muduo_base 
    pthread
)