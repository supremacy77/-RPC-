// src/TcpConnection.h
#pragma once
#include "Buffer.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include <memory>
#include <string>

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*)>;
    using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;

    TcpConnection(EventLoop* loop, int fd, const std::string& name);
    ~TcpConnection();

    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = std::move(cb); }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = std::move(cb); }

    void connectEstablished();   // 刚开始时调用，启动读监听
    void send(const std::string& msg);
    void shutdown();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();

    EventLoop* loop_;
    std::string name_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
