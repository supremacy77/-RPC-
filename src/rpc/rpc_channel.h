#pragma once
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <future>
#include <functional>
#include "rpc_header.pb.h"
#include "rpc_codec.h"
#include "load_balancer.h"
#include "registry_client.h"
#include "../TcpConnection.h"
#include "../EventLoop.h"
#include "custom_controller.h"

class RpcChannel : public google::protobuf::RpcChannel {
public:
    RpcChannel(std::shared_ptr<RegistryClient> registryClient,
               const std::string& serviceName,
               LoadBalanceType lbType = LoadBalanceType::ROUND_ROBIN);
    ~RpcChannel();

    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController* controller,
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    google::protobuf::Closure* done) override;

    void close();

private:
    // 内部结构
    struct PendingSync {
        std::promise<std::shared_ptr<rpc::RpcResponse>> promise;
    };
    struct PendingAsync {
        google::protobuf::Message* response;
        google::protobuf::Closure* done;
        CustomController* controller;
    };

    std::shared_ptr<TcpConnection> getConnection(const std::string& addr);
    bool doCall(const google::protobuf::MethodDescriptor* method,
                CustomController* controller,
                const google::protobuf::Message* request,
                google::protobuf::Message* response,
                int timeoutMs);
    void doCallAsync(const google::protobuf::MethodDescriptor* method,
                     CustomController* controller,
                     const google::protobuf::Message* request,
                     google::protobuf::Message* response,
                     google::protobuf::Closure* done);
    void onRpcResponse(const TcpConnectionPtr& conn, Buffer* buf);
    void removePending(uint64_t id);

    std::shared_ptr<RegistryClient> registryClient_;
    std::string serviceName_;
    LoadBalancer loadBalancer_;

    std::mutex connMutex_;
    std::unordered_map<std::string, std::shared_ptr<TcpConnection>> connections_;

    std::mutex pendingMutex_;
    std::unordered_map<uint64_t, std::unique_ptr<PendingSync>> pendingSync_;
    std::unordered_map<uint64_t, PendingAsync> pendingAsync_;

    std::atomic<uint64_t> nextId_{0};
    RpcCodec codec_;
};
