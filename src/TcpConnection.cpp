// src/TcpConnection.cpp
#include "TcpConnection.h"
#include <unistd.h>
#include <cassert>
#include <iostream>

TcpConnection::TcpConnection(EventLoop* loop, int fd, const std::string& name)
    : loop_(loop),
      name_(name),
      socket_(new Socket(fd)),
      channel_(new Channel(loop, fd))
{
    channel_->setReadCallback([this]{ handleRead(); });
    channel_->setWriteCallback([this]{ handleWrite(); });
    channel_->setCloseCallback([this]{ handleClose(); });
    channel_->setErrorCallback([this]{ handleClose(); });
}

TcpConnection::~TcpConnection() {
    // 确保 Channel 移除（如果还没移除的话）
    // 智能指针销毁时 Socket 和 Channel 自动销毁
}

void TcpConnection::connectEstablished() {
    channel_->enableReading(); // 注册到 epoll，开始监听读事件
}

void TcpConnection::handleRead() {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        // 读到数据，触发消息回调
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_);
        }
    } else if (n == 0) {
        handleClose();  // 对端关闭连接（read 返回 0）
    } else {
        // n < 0，错误（如 EAGAIN 在 LT 下通常不会出现，可忽略或记录）
        if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK) {
            std::cerr << "TcpConnection::handleRead error\n";
            handleClose();
        }
    }
}

void TcpConnection::handleWrite() {
    // 将输出缓冲区的数据尽量写出去
    if (outputBuffer_.readableBytes() > 0) {
        ssize_t n = write(channel_->fd(),
                          outputBuffer_.peek(),
                          outputBuffer_.readableBytes());
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting(); // 写完了，取消写事件监听
            }
        }
    } else {
        channel_->disableWriting();
    }
}

void TcpConnection::handleClose() {
    // 通知上层（EchoServer）移除本连接
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
    // 对象可能在此回调中被销毁，不再访问成员
}

void TcpConnection::send(const std::string& msg) {
    // 如果输出缓冲区为空，尝试直接发送，减少一次拷贝
    if (outputBuffer_.readableBytes() == 0 && msg.size() > 0) {
        ssize_t n = write(channel_->fd(), msg.data(), msg.size());
        if (n >= 0) {
            if (static_cast<size_t>(n) < msg.size()) {
                // 只发出了部分，剩下的放入输出缓冲区并开启写监听
                outputBuffer_.append(msg.data() + n, msg.size() - n);
                channel_->enableWriting();
            }
            return; // 全部发出，直接返回
        }
        // 写失败（比如缓冲区满），走下面的缓冲流程
    }
    // 将数据放入输出缓冲区，并注册 EPOLLOUT 等待可写
    outputBuffer_.append(msg.data(), msg.size());
    channel_->enableWriting();
}

void TcpConnection::shutdown() {
    ::shutdown(socket_->fd(), SHUT_WR);
}
