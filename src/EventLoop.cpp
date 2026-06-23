// src/EventLoop.cpp
#include "EventLoop.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cassert>
#include <iostream>

namespace {
int createEventfd() {
    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        perror("eventfd");
        exit(1);
    }
    return fd;
}
}

EventLoop::EventLoop()
    : epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(16),                // 初始给 16 个事件空间
      looping_(false),
      quit_(false),
      threadId_(std::this_thread::get_id()),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    if (epollfd_ < 0) { perror("epoll_create1"); exit(1); }
    // 唤醒 Channel 读到数据后只需把数据读出，无实际业务
    wakeupChannel_->setReadCallback([this]{
        uint64_t one = 1;
        ssize_t n = read(wakeupFd_, &one, sizeof one);
        (void)n; // 忽略返回值
    });
    wakeupChannel_->enableReading(); // 注册到 epoll
}

EventLoop::~EventLoop() {
    wakeupChannel_->remove();
    close(wakeupFd_);
    close(epollfd_);
}

void EventLoop::loop() {
    assert(!looping_);
    looping_ = true;
    while (!quit_) {
        int numEvents = epoll_wait(epollfd_, events_.data(), events_.size(), 1000);
        if (numEvents > 0) {
            for (int i = 0; i < numEvents; ++i) {
                Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
                ch->set_revents(events_[i].events);
                ch->handleEvent();
            }
            // 如果返回的事件数等于当前数组大小，说明可能不够，扩容
            if (static_cast<size_t>(numEvents) == events_.size()) {
                events_.resize(events_.size() * 2);
            }
        }
        doPendingFunctors();  // 处理在其他线程提交的任务
    }
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup(); // 若在其他线程调用 quit，需要唤醒 loop 使其退出
    }
}

void EventLoop::updateChannel(Channel* ch) {
    int fd = ch->fd();
    struct epoll_event event;
    event.events = ch->events();
    event.data.ptr = ch;      // 把 Channel 指针存入 epoll_event，方便回调

    if (channels_.find(fd) == channels_.end()) {
        channels_[fd] = ch;
        epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event);
    } else {
        epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
    }
}

void EventLoop::removeChannel(Channel* ch) {
    int fd = ch->fd();
    channels_.erase(fd);
    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr);
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();   // 如果就在 I/O 线程，直接执行
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    // 如果不在 I/O 线程调用，或者当前正在调用 doPendingFunctors，需要唤醒
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    (void)n;
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); // 高效交换，减少锁持有时间
    }
    for (const auto& func : functors) {
        func();
    }
}
