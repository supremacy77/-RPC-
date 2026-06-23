// src/Channel.h
#pragma once
#include <functional>
#include <sys/epoll.h>

class EventLoop;  // 前向声明

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent();   // 由 EventLoop 调用，根据 revents_ 分发

    // 设置各种回调
    void setReadCallback(EventCallback cb)  { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // epoll 返回时设置

    // 启用/禁用读写监听
    void enableReading();
    void enableWriting();
    void disableWriting();
    bool isWriting() const { return events_ & EPOLLOUT; }

    void remove();               // 取消所有事件并从 epoll 移除
    EventLoop* ownerLoop() { return loop_; }

private:
    void update();               // 向 EventLoop 申请更新 epoll 事件

    EventLoop* loop_;
    int fd_;
    uint32_t events_ = 0;        // 我们关心的事件
    uint32_t revents_ = 0;       // epoll 返回的就绪事件

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
