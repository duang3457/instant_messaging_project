#!/bin/sh
SRC_DIR=./
DST_DIR=./

#C++
# mkdir -p $DST_DIR/cpp
protoc -I=$SRC_DIR \
  --cpp_out=$DST_DIR \
  --grpc_out=$DST_DIR \
  --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
  $SRC_DIR/*.proto


# 命令解释
# -I=$SRC_DIR：指定 .proto 文件的搜索路径，这里 $SRC_DIR 是 .proto 文件所在的目录。
# --cpp_out=$DST_DIR：指定生成的 C++ 代码的输出目录，$DST_DIR 是输出目录。
# --grpc_out=$DST_DIR：指定生成的 gRPC 相关代码的输出目录，同样是 $DST_DIR。
# --plugin=protoc-gen-grpc=which grpc_cpp_plugin``：指定 gRPC 插件的路径。which grpc_cpp_plugin 会自动查找系统中 grpc_cpp_plugin 的路径，并将其作为插件传递给 protoc。
# $SRC_DIR/*.proto：指定要处理的 .proto 文件，这里使用通配符 *.proto 表示处理 $SRC_DIR 目录下的所有 .proto 文件。