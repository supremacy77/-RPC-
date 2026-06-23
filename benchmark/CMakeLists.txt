cmake_minimum_required(VERSION 3.10)
project(rpc_benchmark CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -O2 -pthread")

include_directories(
    ${PROJECT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}/../src/rpc
    ${PROJECT_SOURCE_DIR}/../third_party
)

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

add_executable(rpc_benchmark
    rpc_benchmark.cpp
    ${PROJECT_SOURCE_DIR}/../src/Buffer.cpp
    ${PROJECT_SOURCE_DIR}/../src/rpc/rpc_header.pb.cc
    ${PROJECT_SOURCE_DIR}/../src/math_service.pb.cc
)

target_link_libraries(rpc_benchmark
    ${Protobuf_LIBRARIES}
)
