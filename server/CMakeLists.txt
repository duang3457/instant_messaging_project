cmake_minimum_required(VERSION 3.10)
project(ChatRoom)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

# 添加子目录
ADD_SUBDIRECTORY(proto)        # 先构建共享的 proto 库
ADD_SUBDIRECTORY(application)