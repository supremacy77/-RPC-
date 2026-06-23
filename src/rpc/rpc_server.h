// src/rpc/rpc_server.h
#pragma once
#include "EventLoop.h"
#include "Acceptor.h"
#include "TcpConnection.h"
#include "rpc_header.pb.h"
#include "rpc_service_registry.h"
#include "rpc_codec.h"
#include <memory>
#include <unordered_set>
#include "ThreadPool.h"

class RpcServer {
public:
    RpcServer(EventLoop* loop, const InetAddress& listenAddr, int nums = 4);
    void start();
    RpcServiceRegistry& registry() { return registry_; }

private:
    rpc::RpcResponse dispatchRequest(const rpc::RpcRequest& request);
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf);
    void dummyCallback();
    EventLoop* loop_;
    std::unique_ptr<Acceptor> acceptor_;
    RpcServiceRegistry registry_;
    std::unordered_set<TcpConnectionPtr> connections_;
    ThreadPool threadPool_;
};
