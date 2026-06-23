// src/Channel.cpp
#include "Channel.h"
#include "EventLoop.h"
#include <cassert>

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd) {}

Channel::~Channel() {
    remove();
}

void Channel::enableReading() {
    events_ |= EPOLLIN;
    update();
}

void Channel::enableWriting() {
    events_ |= EPOLLOUT;
    update();
}

void Channel::disableWriting() {
    events_ &= ~EPOLLOUT;
    update();
}

void Channel::remove() {
    events_ = 0;
    update();
}

void Channel::update() {
    loop_->updateChannel(this);
}

void Channel::handleEvent() {
    // 处理挂起事件：如果对端关闭且没有数据可读，调用关闭回调
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
        return;
    }
    // 错误
    if (revents_ & (EPOLLERR | EPOLLRDHUP)) {
        if (errorCallback_) errorCallback_();
        return;
    }
    if (revents_ & EPOLLIN) {
        if (readCallback_) readCallback_();
    }
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}
