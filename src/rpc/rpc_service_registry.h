// src/rpc/rpc_service_registry.h
#pragma once
#include <string>
#include <unordered_map>
#include <google/protobuf/service.h>

// 维护服务名到服务对象的映射，服务端用
class RpcServiceRegistry {
public:
    void registerService(google::protobuf::Service* service) {
        const std::string& name = service->GetDescriptor()->full_name();
        services_[name] = service;
    }

    google::protobuf::Service* findService(const std::string& name) const {
        auto it = services_.find(name);
        return (it != services_.end()) ? it->second : nullptr;
    }

private:
    std::unordered_map<std::string, google::protobuf::Service*> services_;
};
