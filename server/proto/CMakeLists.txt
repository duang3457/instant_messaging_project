# 生成 protobuf/grpc 代码的共享库
cmake_minimum_required(VERSION 3.10)

# 查找 Protobuf 和 gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

message(STATUS "Generating ChatRoom proto library...")

# 定义所有 proto 文件（使用绝对路径）
set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Protocol.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Comet.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Job.proto
)

# 生成输出文件路径
set(PROTO_PROTOCOL_SRCS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Protocol.pb.cc")
set(PROTO_PROTOCOL_HDRS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Protocol.pb.h")

set(PROTO_COMET_SRCS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Comet.pb.cc")
set(PROTO_COMET_HDRS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Comet.pb.h")
set(GRPC_COMET_SRCS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Comet.grpc.pb.cc")
set(GRPC_COMET_HDRS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Comet.grpc.pb.h")

set(PROTO_JOB_SRCS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Job.pb.cc")
set(PROTO_JOB_HDRS "${CMAKE_CURRENT_BINARY_DIR}/ChatRoom.Job.pb.h")

# Protocol.proto (没有 gRPC service)
add_custom_command(
    OUTPUT ${PROTO_PROTOCOL_SRCS} ${PROTO_PROTOCOL_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         -I${CMAKE_CURRENT_SOURCE_DIR}
         ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Protocol.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Protocol.proto
    COMMENT "Generating ChatRoom.Protocol protobuf files"
    VERBATIM
)

# Comet.proto (包含 gRPC service)
add_custom_command(
    OUTPUT ${PROTO_COMET_SRCS} ${PROTO_COMET_HDRS} ${GRPC_COMET_SRCS} ${GRPC_COMET_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         -I${CMAKE_CURRENT_SOURCE_DIR}
         ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Comet.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Comet.proto
            ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Protocol.proto
    COMMENT "Generating ChatRoom.Comet protobuf/grpc files"
    VERBATIM
)

# Job.proto (没有 gRPC service)
add_custom_command(
    OUTPUT ${PROTO_JOB_SRCS} ${PROTO_JOB_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         -I${CMAKE_CURRENT_SOURCE_DIR}
         ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Job.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ChatRoom.Job.proto
    COMMENT "Generating ChatRoom.Job protobuf files"
    VERBATIM
)

# 创建共享的 proto 静态库
add_library(chatroom_proto STATIC 
    ${PROTO_PROTOCOL_SRCS} ${PROTO_PROTOCOL_HDRS}
    ${PROTO_COMET_SRCS} ${PROTO_COMET_HDRS} ${GRPC_COMET_SRCS} ${GRPC_COMET_HDRS}
    ${PROTO_JOB_SRCS} ${PROTO_JOB_HDRS}
)

# 设置包含目录（PUBLIC 以便链接此库的目标也能访问）
target_include_directories(chatroom_proto PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# 链接所需的库
target_link_libraries(chatroom_proto PUBLIC 
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
)

message(STATUS "ChatRoom proto library target created: chatroom_proto")
