#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "noncopyable.h"
class EventLoop;
class EventLoopThread;
// 实现了 Muduo 网络库中的 反应堆线程池（Reactor Thread Pool）。
// 其核心职责是：根据用户配置的线程数，创建并管理多个 EventLoopThread 线程；当新客户端连接到来时，采用 Round-Robin（轮询）算法挑选一个工作线程（SubLoop）来接管该连接。
class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg); // 构造函数：必须传入一个 baseLoop_（即 MainLoop/主 Reactor）
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops(); // 获取所有的EventLoop

    bool started() const { return started_; } // 是否已经启动
    const std::string name() const { return name_; } // 获取名字

private:
    EventLoop *baseLoop_; // 用户使用muduo创建的loop 如果线程数为1 那直接使用用户创建的loop 否则创建多EventLoop     主线程/主 Loop (MainReactor)，专职负责 listen 和 accept
    std::string name_;//线程池名称，通常由用户指定，线程池中EventLoopThread名称依赖于线程池名称。
    bool started_;//是否已经启动标志
    int numThreads_;//线程池中线程的数量
    int next_; // 新连接到来，所选择EventLoop的索引
    std::vector<std::unique_ptr<EventLoopThread>> threads_;//IO线程的列表
    std::vector<EventLoop *> loops_;//线程池中EventLoop的列表，指向的是EventLoopThread线程函数创建的EventLoop对象。
};