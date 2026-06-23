cmake_minimum_required(VERSION 3.10)
project(myreactor_rpc CXX)

# ========== 全局编译配置 ==========
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -O0 -pthread")

# ========== 头文件搜索路径 ==========
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/rpc
    ${PROJECT_SOURCE_DIR}/third_party
)

# ========== 查找 Protobuf 依赖 ==========
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# ========== 1. 注册中心 HTTP 服务 ==========
add_executable(registry_center
    src/registry_center_main.cpp
    src/rpc/registry_center.cpp
)
target_link_libraries(registry_center
    ${Protobuf_LIBRARIES}
)

# ========== 2. RPC 服务端 ==========
add_executable(rpc_server
    src/rpc_server_main.cpp

    # Reactor 网络库
    src/Acceptor.cpp
    src/Buffer.cpp
    src/Channel.cpp
    src/EventLoop.cpp
    src/InetAddress.cpp
    src/Socket.cpp
    src/TcpConnection.cpp
    src/ThreadPool.cpp

    # RPC 框架核心
    src/rpc/rpc_server.cpp
    src/rpc/rpc_server_handler.cpp
    src/rpc/rpc_channel.cpp
    src/rpc/registry_center.cpp
    src/rpc/registry_client.cpp

    # Protobuf 生成文件
    src/math_service.pb.cc
    src/rpc/rpc_header.pb.cc
)
target_link_libraries(rpc_server
    ${Protobuf_LIBRARIES}
)

# ========== 3. RPC 客户端 ==========
add_executable(rpc_client
    src/rpc_client_main.cpp

    # Reactor 网络库
    src/Buffer.cpp
    src/Channel.cpp
    src/EventLoop.cpp
    src/InetAddress.cpp
    src/Socket.cpp
    src/TcpConnection.cpp

    # RPC 客户端核心
    src/rpc/rpc_channel.cpp
    src/rpc/registry_client.cpp

    # Protobuf 生成文件
    src/math_service.pb.cc
    src/rpc/rpc_header.pb.cc
)
target_link_libraries(rpc_client
    ${Protobuf_LIBRARIES}
)
