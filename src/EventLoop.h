// src/EventLoop.h
#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include "Channel.h"

class EventLoop {
 public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();              // 主循环，阻塞运行
    void quit();              // 退出循环
    void updateChannel(Channel* ch);
    void removeChannel(Channel* ch);

    // 在 I/O 线程中执行任务（如果跨线程会唤醒）
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }

private:
    void doPendingFunctors();
    void wakeup();

    int epollfd_;
    std::vector<struct epoll_event> events_;   // 传给 epoll_wait 的数组
    std::unordered_map<int, Channel*> channels_; // fd -> Channel

    bool looping_;
    bool quit_;
    std::thread::id threadId_;

    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;    // 待执行的任务

    int wakeupFd_;                             // eventfd，用于唤醒
    std::unique_ptr<Channel> wakeupChannel_;   // 唤醒用的 Channel
};
