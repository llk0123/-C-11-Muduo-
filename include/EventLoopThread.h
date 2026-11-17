#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "noncopyable.h"
#include "Thread.h"

class EventLoop;
// 保主线程在调用 startLoop() 时，能够安全、同步地获取新线程中创建好的 EventLoop 指针。
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>; // 线程初始化完成后的回调函数签名（例如在 loop() 开启前做某些线程特有的初始化）

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();
    // 启动新线程并阻塞等待，直到新线程创建好 EventLoop 后返回其指针
    EventLoop *startLoop();

private:
    void threadFunc();

    EventLoop *loop_;              // 指向新线程中创建的 EventLoop 对象的指针（受 mutex_ 保护）
    bool exiting_;                 // 标识线程是否正在退出
    Thread thread_;                // 底层封装的 pthread / std::thread 对象
    std::mutex mutex_;             // 互斥锁
    std::condition_variable cond_; // 条件变量
    ThreadInitCallback callback_;  // 用户自定义的线程初始化回调
};