// src/Acceptor.cpp
#include "Acceptor.h"
#include <iostream>

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      listenSock_(),
      acceptChannel_(loop, listenSock_.fd())
{
    listenSock_.bind(listenAddr);
    acceptChannel_.setReadCallback([this]{ handleRead(); });
}

void Acceptor::listen() {
    listenSock_.listen();
    acceptChannel_.enableReading();  // 将监听 fd 注册到 epoll，监听读事件
}

void Acceptor::setNewConnectionCallback(const NewConnectionCallback& cb) { 
	newConnectionCallback_ = cb;
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = listenSock_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    }
    // 如果 connfd < 0，说明暂时没有更多连接，忽略即可（LT模式下）
}
