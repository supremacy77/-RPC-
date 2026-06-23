// src/rpc/rpc_server.cpp
#include "rpc_server.h"
#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <iostream>

RpcServer::RpcServer(EventLoop* loop, const InetAddress& listenAddr, int nums)
    : loop_(loop)
    , acceptor_(new Acceptor(loop, listenAddr))
    , threadPool_(static_cast<size_t>(nums))
{
    acceptor_->setNewConnectionCallback(
        [this](int sockfd, const InetAddress& peer) {
            TcpConnectionPtr conn = std::make_shared<TcpConnection>(
                loop_, sockfd, peer.toIpPort());
            connections_.insert(conn);
            conn->setMessageCallback(
                [this](const TcpConnectionPtr& c, Buffer* buf) {
                    onMessage(c, buf);
                });
            conn->setCloseCallback(
                [this](const TcpConnectionPtr& c) {
                    connections_.erase(c);
                });
            conn->connectEstablished();
        });
}

void RpcServer::start() {
    acceptor_->listen();
}

void RpcServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
    // 循环解出所有完整消息
    while (true) {
        uint64_t reqId = 0;
        std::string msgData = RpcCodec::tryUnpackWithId(buf, reqId);
        if (msgData.empty()) break;

        // 解析请求
        auto request = std::make_shared<rpc::RpcRequest>();
        if (!request->ParseFromString(msgData)) {
            // 协议错误，立即在 I/O 线程发送错误响应（带上 id）
            rpc::RpcResponse response;
            response.set_error_code(rpc::RpcResponse::WRONG_PROTO);
            conn->send(RpcCodec::packWithId(reqId, response.SerializeAsString()));
            continue;
        }

        // 捕获 conn、request 和 reqId，提交到线程池
        threadPool_.enqueue([this, conn, request, reqId]() {
            // 1. 在工作线程中执行反射调用（可能耗时）
            rpc::RpcResponse response = this->dispatchRequest(*request);

            // 2. 序列化响应
            std::string serializedResp = response.SerializeAsString();

            // 3. 回到 I/O 线程发送响应（带上 id）
            conn->getLoop()->runInLoop([conn, serializedResp, reqId]() {
                conn->send(RpcCodec::packWithId(reqId, serializedResp));
            });
        });
    }
}

void RpcServer::dummyCallback() {
    // 空实现即可
}

rpc::RpcResponse RpcServer::dispatchRequest(const rpc::RpcRequest& request) {
    rpc::RpcResponse response;
    google::protobuf::Service* service = registry_.findService(request.service_name());
    if (!service) {
        response.set_error_code(rpc::RpcResponse::NO_SERVICE);
        return response;
    }

    const google::protobuf::MethodDescriptor* method =
        service->GetDescriptor()->FindMethodByName(request.method_name());
    if (!method) {
        response.set_error_code(rpc::RpcResponse::NO_METHOD);
        return response;
    }

    // 根据方法描述符创建请求和响应实例
    std::unique_ptr<google::protobuf::Message> req_msg(
        service->GetRequestPrototype(method).New());
    if (!req_msg->ParseFromString(request.args())) {
        response.set_error_code(rpc::RpcResponse::INVALID_ARGUMENT);
        return response;
    }

    std::unique_ptr<google::protobuf::Message> resp_msg(
        service->GetResponsePrototype(method).New());

    // ----- 新增：捕获业务方法抛出的异常 -----
    try {
        service->CallMethod(method, nullptr, req_msg.get(), resp_msg.get(),
                            google::protobuf::NewCallback<RpcServer>(this, &RpcServer::dummyCallback));
    } catch (const std::exception& e) {
        response.set_error_code(rpc::RpcResponse::INTERNAL_ERROR);
        response.set_error_msg(std::string("Service exception: ") + e.what());
        return response;
    } catch (...) {
        response.set_error_code(rpc::RpcResponse::INTERNAL_ERROR);
        response.set_error_msg("Unknown service exception");
        return response;
    }
    // --------------------------------------

    response.set_error_code(rpc::RpcResponse::NO_ERROR);
    response.set_result(resp_msg->SerializeAsString());
    return response;
}
