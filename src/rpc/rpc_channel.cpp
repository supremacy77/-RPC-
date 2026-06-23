#include "rpc_channel.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <thread>
#include <iostream>
#include "../Socket.h"
#include "../InetAddress.h"

using namespace std::chrono;

// ---------- 工具函数：创建一个非阻塞 TCP 连接 ----------
static int createNonBlockingConnection(const std::string& ip, uint16_t port) {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int ret = ::connect(sockfd, (struct sockaddr*)&addr, sizeof addr);
    if (ret < 0 && errno != EINPROGRESS) {
        ::close(sockfd);
        return -1;
    }
    return sockfd;
}

// ---------- RpcChannel 实现 ----------
RpcChannel::RpcChannel(std::shared_ptr<RegistryClient> registryClient,
                       const std::string& serviceName,
                       LoadBalanceType lbType)
    : registryClient_(std::move(registryClient)),
      serviceName_(serviceName),
      loadBalancer_(lbType) {
}

RpcChannel::~RpcChannel() {
    close();
}

void RpcChannel::close() {
    std::lock_guard<std::mutex> lock(connMutex_);
    for (auto& [_, conn] : connections_) {
        if (conn) conn->shutdown();
    }
    connections_.clear();
}

std::shared_ptr<TcpConnection> RpcChannel::getConnection(const std::string& addr) {
    // 先检查已有连接
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(addr);
        if (it != connections_.end() && it->second) {
            return it->second;   // 存在即返回（不检查 connected，因为库无此接口）
        }
    }

    // 解析地址
    size_t colon = addr.find(':');
    if (colon == std::string::npos) return nullptr;
    std::string ip = addr.substr(0, colon);
    uint16_t port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)));

    int sockfd = createNonBlockingConnection(ip, port);
    if (sockfd < 0) return nullptr;

    // 客户端需要一个 EventLoop 驱动，我们为 RpcChannel 创建一个专属线程
    // 如果多个连接共享一个 loop 更好，但简单起见每个通道一个 EventLoop
    static std::mutex loopMutex;
    static std::unique_ptr<EventLoop> clientLoop;
    {
        std::lock_guard<std::mutex> lock(loopMutex);
        if (!clientLoop) {
            clientLoop = std::make_unique<EventLoop>();
            std::thread([](EventLoop* loop) { loop->loop(); }, clientLoop.get()).detach();
        }
    }

    // 创建 TcpConnection，使用你已有的三参数构造函数
    auto conn = std::make_shared<TcpConnection>(clientLoop.get(), sockfd, addr);
    conn->setMessageCallback(
        [this](const TcpConnectionPtr& c, Buffer* buf) {
            onRpcResponse(c, buf);
        });
    conn->connectEstablished();   // 注册事件

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connections_[addr] = conn;
    }
    return conn;
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {
    auto* ctl = dynamic_cast<CustomController*>(controller);
    if (!ctl) {
        controller->SetFailed("controller must be CustomController");
        if (done) done->Run();
        return;
    }

    if (done) {
        doCallAsync(method, ctl, request, response, done);
        return;
    }

    // 同步调用，带重试
    int maxRetries = ctl->maxRetries();
    bool idempotent = ctl->isIdempotent();
    for (int i = 0; i <= maxRetries; ++i) {
        if (i > 0 && !idempotent) break;
        bool ok = doCall(method, ctl, request, response, ctl->timeoutMs());
        if (ok) return;
        ctl->Reset();   // 清除错误状态，准备重试
    }
    ctl->SetFailed("all retries exhausted");
}

bool RpcChannel::doCall(const google::protobuf::MethodDescriptor* method,
                        CustomController* controller,
                        const google::protobuf::Message* request,
                        google::protobuf::Message* response,
                        int timeoutMs) {
    auto addrs = registryClient_->getAddresses(serviceName_);
    if (addrs.empty()) {
        controller->SetFailed("no available servers");
        return false;
    }

    std::string addr = loadBalancer_.select(addrs);
    auto conn = getConnection(addr);
    if (!conn) {
        controller->SetFailed("connection failed");
        return false;
    }

    // 构造 RpcRequest，通过 RpcCodec 打包（带 id）
    uint64_t id = nextId_.fetch_add(1);
    rpc::RpcRequest rpcReq;
    rpcReq.set_service_name(method->service()->full_name());
    rpcReq.set_method_name(method->name());
    rpcReq.set_args(request->SerializeAsString());
    std::string packed = RpcCodec::packWithId(id, rpcReq.SerializeAsString());

    auto sync = std::make_unique<PendingSync>();
    std::future<std::shared_ptr<rpc::RpcResponse>> future = sync->promise.get_future();
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingSync_[id] = std::move(sync);
    }

    conn->send(packed);

    // 等待响应或超时
    auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
    if (status == std::future_status::timeout) {
        removePending(id);
        controller->SetFailed("request timeout");
        return false;
    }

    auto rspPtr = future.get();
    if (!rspPtr) {
        controller->SetFailed("null response");
        return false;
    }
    if (rspPtr->error_code() != rpc::RpcResponse::NO_ERROR) {
        controller->SetFailed(rspPtr->error_msg());
        return false;
    }
    response->ParseFromString(rspPtr->result());
    return true;
}

void RpcChannel::doCallAsync(const google::protobuf::MethodDescriptor* method,
                             CustomController* controller,
                             const google::protobuf::Message* request,
                             google::protobuf::Message* response,
                             google::protobuf::Closure* done) {
    auto addrs = registryClient_->getAddresses(serviceName_);
    if (addrs.empty()) {
        controller->SetFailed("no servers");
        done->Run();
        return;
    }

    std::string addr = loadBalancer_.select(addrs);
    auto conn = getConnection(addr);
    if (!conn) {
        controller->SetFailed("connect failed");
        done->Run();
        return;
    }

    uint64_t id = nextId_.fetch_add(1);
    rpc::RpcRequest rpcReq;
    rpcReq.set_service_name(method->service()->full_name());
    rpcReq.set_method_name(method->name());
    rpcReq.set_args(request->SerializeAsString());
    std::string packed = RpcCodec::packWithId(id, rpcReq.SerializeAsString());

    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingAsync_[id] = {response, done, controller};
    }
    conn->send(packed);
}

void RpcChannel::onRpcResponse(const TcpConnectionPtr& conn, Buffer* buf) {
    // 使用 RpcCodec::tryUnpackWithId 获取完整响应包和 id
    uint64_t id = 0;
    std::string packedData = RpcCodec::tryUnpackWithId(buf, id);
    if (packedData.empty()) return;

    rpc::RpcResponse rsp;
    if (!rsp.ParseFromString(packedData)) {
        std::cerr << "RpcChannel: failed to parse response" << std::endl;
        return;
    }

    // 先处理同步调用
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        auto itSync = pendingSync_.find(id);
        if (itSync != pendingSync_.end()) {
            auto rspPtr = std::make_shared<rpc::RpcResponse>(rsp);
            itSync->second->promise.set_value(rspPtr);
            pendingSync_.erase(itSync);
            return;
        }
    }

    // 再处理异步调用
    // 为了避免回调中加锁造成死锁，先取出回调，释放锁后再执行
    PendingAsync pending;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        auto itAsync = pendingAsync_.find(id);
        if (itAsync != pendingAsync_.end()) {
            pending = itAsync->second;
            pendingAsync_.erase(itAsync);
        } else {
            return;   // 未找到匹配的请求
        }
    }

    // 执行回调（无锁）
    if (rsp.error_code() != rpc::RpcResponse::NO_ERROR) {
        pending.controller->SetFailed(rsp.error_msg());
    } else {
        pending.response->ParseFromString(rsp.result());
    }
    pending.done->Run();
}

void RpcChannel::removePending(uint64_t id) {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pendingSync_.erase(id);
    // 异步超时暂不处理
}
