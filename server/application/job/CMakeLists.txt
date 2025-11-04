INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# 启用C++17支持(gRPC和Abseil需要)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

INCLUDE_DIRECTORIES(/usr/local/include)
INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(/usr/include/jsoncpp)

# 源文件
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} MAIN_LIST)


ADD_EXECUTABLE(job ${MAIN_LIST})

# 链接库的顺序很重要
TARGET_LINK_LIBRARIES(job 
    chatroom_proto      # 链接共享的 proto 库
    muduo_net
    muduo_base
    jsoncpp 
    mysqlclient 
    uuid
    hiredis             # Redis 客户端库
    rdkafka 
    rdkafka++ 
    pthread
)




# TARGET_LINK_LIBRARIES(job muduo_net jsoncpp mysqlclient uuid ssl crypto
#  rdkafka  rdkafka++ protobuf  grpc grpc++  grpc++_reflection gpr   pthread
#  address_sorting re2 upb cares z gpr  absl_raw_hash_set absl_hashtablez_sampler absl_hash absl_city absl_low_level_hash absl_random_distributions absl_random_seed_sequences absl_random_internal_pool_urbg absl_random_internal_randen absl_random_internal_randen_hwaes absl_random_internal_randen_hwaes_impl absl_random_internal_randen_slow absl_random_internal_platform absl_random_internal_seed_material absl_random_seed_gen_exception absl_statusor absl_status absl_cord absl_cordz_info absl_cord_internal absl_cordz_functions absl_exponential_biased absl_cordz_handle absl_bad_optional_access absl_str_format_internal absl_synchronization absl_graphcycles_internal absl_stacktrace absl_symbolize absl_debugging_internal absl_demangle_internal absl_malloc_internal absl_time absl_civil_time absl_strings absl_strings_internal rt absl_base absl_spinlock_wait absl_int128 absl_throw_delegate absl_time_zone absl_bad_variant_access absl_raw_logging_internal absl_log_severity
# )
# 如果需要明确指定 OpenSSL 库路径
set(OPENSSL_ROOT_DIR /usr/local/ssl)
set(OPENSSL_CRYPTO_LIBRARY /usr/local/lib/libcrypto.so)
set(OPENSSL_SSL_LIBRARY /usr/local/lib/libssl.so)

  # This branch assumes that gRPC and all its dependencies are already installed
  # on this system, so they can be located by find_package().

  # Find Protobuf installation
  # Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
# 添加 gRPC 和 Protobuf 相关路径
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
if(CMAKE_CROSSCOMPILING)
find_program(_PROTOBUF_PROTOC protoc)
else()
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)
if(CMAKE_CROSSCOMPILING)
find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()



# 链接库的顺序很重要
TARGET_LINK_LIBRARIES(job 
    chatroom_proto      # 链接共享的 proto 库
    muduo_net
    muduo_base
    jsoncpp 
    mysqlclient 
    uuid     
    rdkafka 
    rdkafka++ 
    pthread
)