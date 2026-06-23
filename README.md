# MyReactor RPC Framework

一个基于 C++17 和 Reactor 网络模型的轻量级 RPC 框架，使用 Protobuf 进行序列化，支持服务注册与发现、负载均衡、同步/异步调用。

---

## 架构总览

```
┌─────────────┐         ┌──────────────────┐         ┌─────────────┐
│  RPC Client │◄───────►│  Registry Center │◄───────►│  RPC Server │
│  (rpc_client)│  HTTP   │  (HTTP :8500)    │  HTTP   │ (rpc_server)│
└──────┬──────┘         └──────────────────┘         └──────┬──────┘
       │                                                     │
       │              TCP (长度前缀 + Protobuf)               │
       └─────────────────────────────────────────────────────┘
```

**三个独立进程：**

| 组件 | 端口 | 职责 |
|------|------|------|
| `registry_center` | HTTP :8500 | 服务注册中心，提供 HTTP API 管理服务地址 |
| `rpc_server` | TCP :9999 | RPC 服务端，提供业务方法（如 MathService） |
| `rpc_client` | - | RPC 客户端，发起远程调用 |

---

## 目录结构

```
.
├── CMakeLists.txt
├── third_party/                        # 第三方库（httplib、nlohmann/json）
└── src/
    ├── math_service.proto              # Protobuf 服务定义
    ├── math_service.pb.h / .pb.cc      # Protobuf 生成的代码
    ├── math_service_impl.h             # 业务服务实现（MathService）
    │
    ├── rpc_server_main.cpp             # 服务端入口
    ├── rpc_client_main.cpp             # 客户端入口
    ├── registry_center_main.cpp        # 注册中心入口
    │
    ├── rpc/                            # RPC 框架核心
    │   ├── rpc_server.h / .cpp         # RPC 服务端（接收请求、分发调用）
    │   ├── rpc_channel.h / .cpp        # RPC 客户端通道（发送请求、匹配响应）
    │   ├── rpc_codec.h                 # 编解码器（长度前缀 + id 帧协议）
    │   ├── rpc_header.pb.h / .pb.cc    # RPC 协议头（RpcRequest / RpcResponse）
    │   ├── rpc_service_registry.h      # 服务端本地服务注册表
    │   ├── registry_client.h / .cpp    # 客户端注册中心访问（HTTP + 磁盘缓存）
    │   ├── registry_center.h / .cpp    # 注册中心核心逻辑（内存 + 磁盘持久化）
    │   ├── load_balancer.h             # 负载均衡（Round Robin / Random）
    │   └── custom_controller.h         # 自定义 RPC 控制器（超时、重试、幂等）
    │
    ├── Acceptor.h / .cpp               # Reactor: 监听新连接
    ├── Buffer.h / .cpp                 # Reactor: 非阻塞 I/O 缓冲区
    ├── Channel.h / .cpp                # Reactor: epoll 事件分发
    ├── EventLoop.h / .cpp              # Reactor: 事件循环
    ├── InetAddress.h / .cpp            # Reactor: 网络地址封装
    ├── Socket.h / .cpp                 # Reactor: socket fd 封装
    ├── TcpConnection.h / .cpp          # Reactor: TCP 连接管理
    └── ThreadPool.h / .cpp             # Reactor: 线程池
```

---

## 环境要求

- Linux（依赖 `epoll`、`eventfd` 等 Linux 特有 API）
- CMake >= 3.10
- GCC / Clang（支持 C++17）
- Protobuf 3.21+
- pthread

---

## 构建与运行

### 编译

```bash
cmake -B build
cmake --build build
```

### 启动（按顺序）

```bash
# 1. 启动注册中心
./build/registry_center

# 2. 启动 RPC 服务端（会自动向注册中心注册）
./build/rpc_server

# 3. 启动 RPC 客户端（发起调用）
./build/rpc_client
```

---

## 核心组件详解

### 1. Reactor 网络库

基于 epoll 的单线程事件驱动模型，核心组件：

- **EventLoop**：事件循环主引擎，封装 `epoll_wait` + `eventfd` 唤醒机制，支持跨线程任务投递（`runInLoop` / `queueInLoop`）
- **Channel**：将 fd 与回调绑定，管理 epoll 事件注册（读/写）
- **Acceptor**：监听端口，新连接到来时创建 TcpConnection
- **TcpConnection**：封装一个 TCP 连接，提供非阻塞读写
- **Buffer**：应用层缓冲区，支持 scatter-gather I/O（`readv`）
- **ThreadPool**：固定大小线程池，用于执行 RPC 业务逻辑

### 2. RPC 协议

**帧格式：**

```
┌────────────────┬────────────────┬─────────────────────────────┐
│ Length (4 字节) │ ID (8 字节)    │ Protobuf 数据               │
│ 网络序          │ 网络序          │ RpcRequest / RpcResponse    │
└────────────────┴────────────────┴─────────────────────────────┘
```

- `Length`：`ID + Protobuf 数据` 的总字节数
- `ID`：请求-响应匹配标识，客户端生成，服务端原样回传
- `Protobuf 数据`：序列化的 `RpcRequest`（客户端→服务端）或 `RpcResponse`（服务端→客户端）

**RpcRequest：**

```protobuf
message RpcRequest {
    string service_name = 1;  // 如 "MathService"
    string method_name = 2;   // 如 "Add"
    bytes args = 3;           // 序列化的业务请求参数
}
```

**RpcResponse：**

```protobuf
message RpcResponse {
    ErrorCode error_code = 1;
    string error_msg = 2;
    bytes result = 3;         // 序列化的业务响应结果
}
```

### 3. 服务注册与发现

**注册中心（HTTP API）：**

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/register` | 注册服务，Body: `{"service":"MathService","address":"127.0.0.1:9999"}` |
| GET | `/discover?service=MathService` | 发现服务，返回地址数组 |

- 内存存储 + 磁盘持久化（原子写入，先写临时文件再 rename）
- 客户端三级容错：内存缓存 → HTTP 拉取 → 磁盘旧缓存兜底

**客户端注册表（in-process）：**

- `RpcServiceRegistry`：维护 `服务全名 → Service*` 映射
- `RpcServer` 启动时调用 `registry().registerService(&service)` 注册本地服务

### 4. 负载均衡

`LoadBalancer` 支持两种策略：

- **Round Robin**：原子递增取模，线程安全
- **Random**：加锁随机选取

### 5. RPC 调用流程

**同步调用：**

```
Client                              Server
  │                                    │
  │── RpcRequest(service,method,args)──│
  │                                    │── dispatchRequest()
  │                                    │── CallMethod()
  │◄── RpcResponse(result) ───────────│
  │                                    │
```

**带重试的同步调用：**

```cpp
CustomController ctl;
ctl.setTimeoutMs(1000);      // 超时 1 秒
ctl.setMaxRetries(1);         // 最多重试 1 次
ctl.setIdempotent(true);      // 允许重试（幂等操作）

stub.Add(&ctl, &req, &resp, nullptr);
```

**异步调用：**

```cpp
auto* cb = google::protobuf::NewCallback(callbackFunc, resp, ctl);
stub.Add(ctl, &req, resp, cb);
```

---

## 自定义服务

### 步骤 1：定义 Protobuf 文件

```protobuf
syntax = "proto2";
package myservice;

message MyRequest {
    optional string input = 1;
}
message MyResponse {
    optional string output = 1;
}

service MyService {
    rpc MyMethod(MyRequest) returns(MyResponse);
}
```

### 步骤 2：生成代码

```bash
protoc --cpp_out=src/ src/my_service.proto
```

### 步骤 3：实现服务

```cpp
class MyServiceImpl : public ::google::protobuf::Service {
public:
    const ::google::protobuf::ServiceDescriptor* GetDescriptor() override {
        return ::myservice::MyRequest::descriptor()->file()
            ->FindServiceByName("MyService");
    }

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                    ::google::protobuf::RpcController* controller,
                    const ::google::protobuf::Message* request,
                    ::google::protobuf::Message* response,
                    ::google::protobuf::Closure* done) override {
        if (method->name() == "MyMethod") {
            MyMethod(controller,
                     dynamic_cast<const MyRequest*>(request),
                     dynamic_cast<MyResponse*>(response), done);
        }
    }

    void MyMethod(RpcController* controller,
                  const MyRequest* request,
                  MyResponse* response,
                  Closure* done) {
        response->set_output("hello " + request->input());
        if (done) done->Run();
    }

    const ::google::protobuf::Message& GetRequestPrototype(
        const ::google::protobuf::MethodDescriptor* method) const override {
        static MyRequest req;
        return req;
    }

    const ::google::protobuf::Message& GetResponsePrototype(
        const ::google::protobuf::MethodDescriptor* method) const override {
        static MyResponse rsp;
        return rsp;
    }
};
```

### 步骤 4：注册并启动

```cpp
MyServiceImpl myService;
rpcServer.registry().registerService(&myService);
rpcServer.start();
```

---

## 关键类说明

| 类 | 文件 | 职责 |
|----|------|------|
| `EventLoop` | `EventLoop.h` | epoll 事件循环，支持跨线程任务投递 |
| `TcpConnection` | `TcpConnection.h` | TCP 连接封装，非阻塞读写 |
| `Buffer` | `Buffer.h` | 应用层缓冲区，支持 prepend 和 scatter-gather |
| `RpcServer` | `rpc_server.h` | RPC 服务端，接收请求、线程池分发、回送响应 |
| `RpcChannel` | `rpc_channel.h` | RPC 客户端通道，实现 `google::protobuf::RpcChannel` |
| `RpcCodec` | `rpc_codec.h` | 长度前缀 + id 帧编解码 |
| `CustomController` | `custom_controller.h` | 自定义 RPC 控制器，支持超时、重试、幂等配置 |
| `LoadBalancer` | `load_balancer.h` | 负载均衡（Round Robin / Random） |
| `RegistryCenter` | `registry_center.h` | 注册中心核心（内存 + 磁盘持久化） |
| `RegistryClient` | `registry_client.h` | 客户端注册中心访问（HTTP + 三级缓存容错） |
| `ThreadPool` | `ThreadPool.h` | 固定大小线程池 |

---

## License

MIT
