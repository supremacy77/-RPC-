// src/rpc/rpc_server_handler.cpp
#include "rpc_header.pb.h"
#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include "rpc_service_registry.h"

// 处理 RPC 请求，生成响应
rpc::RpcResponse dispatchRequest(const rpc::RpcRequest& request,
                                 const RpcServiceRegistry& registry) {
    rpc::RpcResponse response;
    auto* service = registry.findService(request.service_name());
    if (!service) {
        response.set_error_code(rpc::RpcResponse::NO_SERVICE);
        return response;
    }

    const auto* method = service->GetDescriptor()->FindMethodByName(request.method_name());
    if (!method) {
        response.set_error_code(rpc::RpcResponse::NO_METHOD);
        return response;
    }

    // 根据方法描述符创建请求和响应对象
    std::unique_ptr<google::protobuf::Message> request_msg(
        service->GetRequestPrototype(method).New());
    request_msg->ParseFromString(request.args());

    std::unique_ptr<google::protobuf::Message> response_msg(
        service->GetResponsePrototype(method).New());

    // 调用方法
    service->CallMethod(method, nullptr,
                        request_msg.get(), response_msg.get(),
                        nullptr);

    // 注意：CallMethod 在默认实现中是同步的，但这里为了简化，我们直接使用同步调用。
    // 实际框架应处理异步，但 Protobuf 的默认实现是同步的（用户重写 service 方法）。
    response.set_error_code(rpc::RpcResponse::NO_ERROR);
    response.set_result(response_msg->SerializeAsString());
    return response;
}
