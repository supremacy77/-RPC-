#include "rpc/rpc_server.h"
#include "rpc/registry_client.h"
#include "math_service_impl.h"
#include "httplib.h"   // 为了发送注册请求
#include <iostream>
#include <thread>

int main() {
    // 启动 RPC 服务器
    EventLoop loop;
    InetAddress listenAddr(9999);
    RpcServer rpcServer(&loop, listenAddr);
    MathServiceImpl mathService;
    rpcServer.registry().registerService(&mathService);
    rpcServer.start();
    std::thread loopThread([&loop]() { loop.loop(); });

    // 向注册中心注册
    httplib::Client regClient("127.0.0.1", 8500);
    std::string body = "{\"service\":\"MathService\",\"address\":\"127.0.0.1:9999\"}";
    auto res = regClient.Post("/register", body, "application/json");
    if (res && res->status == 200) {
        std::cout << "Registered to RegistryCenter" << std::endl;
    } else {
        std::cerr << "Failed to register" << std::endl;
    }

    loopThread.join();
    return 0;
}
