// src/Acceptor.h
#pragma once
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <functional>
#include <unistd.h>
class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    void setNewConnectionCallback(const NewConnectionCallback& cb);
    void listen();
private:
    void handleRead();

    EventLoop* loop_;
    Socket listenSock_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
};
