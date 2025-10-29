option(ENABLE_RPC "Enable RPC functionality" OFF)

# 代码内编译开关
if(ENABLE_RPC)
    add_definitions(-DENABLE_RPC)
endif()

# 全局包含目录
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

# 添加子目录
ADD_SUBDIRECTORY(chat-room)  # 服务端
ADD_SUBDIRECTORY(client)     # cpp临时客户端
if(ENABLE_RPC)
    add_subdirectory(logic)  # logic 服务, 业务逻辑 + kafka.Producer
    add_subdirectory(job)    # job 服务, kafka分发服务 : kafka.Comsumer
endif()