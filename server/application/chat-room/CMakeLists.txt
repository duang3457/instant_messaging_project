# chat-room 项目特定的包含目录
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/api)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/mysql)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/redis)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/service)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/rpc)

# 系统库包含目录
INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)

# 收集源文件
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/mysql MYSQL_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/redis REDIS_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/service SERVICE_LIST)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/rpc RPC_LIST)

# 手动添加md5.cc（如果AUX_SOURCE_DIRECTORY没有找到）
set(BASE_LIST ${BASE_LIST} ${CMAKE_CURRENT_SOURCE_DIR}/base/md5.cc)

# 创建可执行文件
if(ENABLE_RPC)
    ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} 
        ${BASE_LIST} ${API_LIST} ${MYSQL_LIST}
        ${REDIS_LIST} ${RPC_LIST})
else()
    ADD_EXECUTABLE(chat-room main.cc  ${SERVICE_LIST} 
        ${BASE_LIST} ${API_LIST} ${MYSQL_LIST}
        ${REDIS_LIST})
endif()

if(ENABLE_RPC)
    # protobuf部分
    set(protobuf_MODULE_COMPATIBLE TRUE) # Protobuf查找方式确保向后兼容
    find_package(Protobuf REQUIRED)
    # find_package(Protobuf) 会设置以下变量：
    # - Protobuf_FOUND          → 是否找到
    # - Protobuf_VERSION        → 版本号
    # - Protobuf_INCLUDE_DIRS   → 头文件路径
    # - Protobuf_LIBRARIES      → 库文件
    # - Protobuf_PROTOC_EXECUTABLE → protoc 编译器路径
    message(STATUS "Using protobuf ${Protobuf_VERSION}")

    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    set(_REFLECTION gRPC::grpc++_reflection)

    if(CMAKE_CROSSCOMPILING)
        find_program(_PROTOBUF_PROTOC protoc)
    else()
        set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
    endif()

    # gRPC部分
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")

    set(_GRPC_GRPCPP gRPC::grpc++)
    if(CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    endif()
endif()

# 添加库搜索路径
LINK_DIRECTORIES(/usr/local/lib)
LINK_DIRECTORIES(/usr/lib/x86_64-linux-gnu)

# 链接库（注意顺序很重要，被依赖的库要放在后面）
if(ENABLE_RPC)
    TARGET_LINK_LIBRARIES(chat-room 
        muduo_net 
        muduo_base
        jsoncpp 
        mysqlclient 
        uuid 
        ssl 
        crypto
        pthread
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
else()
    TARGET_LINK_LIBRARIES(chat-room 
        muduo_net 
        muduo_base 
        jsoncpp 
        mysqlclient 
        uuid 
        ssl 
        crypto 
        pthread
    )
endif()