#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name) // 绑定子线程入口函数 threadFunc
    , mutex_()
    , cond_()
    , callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();  // 1. 跨线程安全地唤醒并退出子线程的事件循环
        thread_.join(); // 2. 等待子线程完全退出，回收线程资源
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启用底层线程Thread类对象thread_中通过start()创建的线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this](){return loop_ != nullptr;}); // 阻塞等待：直到子线程在 threadFunc 中成功创建 EventLoop 并将 loop_ 赋值
        loop = loop_;
    }
    return loop; // 返回新线程中专属的 EventLoop 指针给主线程（例如给 EventLoopThreadPool 收集）
}

// 下面这个方法 是在单独的新线程里运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 1. 创建一个独立的EventLoop对象 和上面的线程是一一对应的 即one loop per thread

    if (callback_)
    {
        callback_(&loop); // 2. 如果设置了初始化回调，在开启事件循环前执行
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop; // 3. 将栈上创建的 loop 地址赋值给成员变量 loop_
        cond_.notify_one(); // 4. 唤醒正卡在 startLoop() 处等待的主线程
    }
    loop.loop();    // 执行EventLoop的loop() 开启了底层的Poller的poll()       5. 核心：子线程陷入 Poller::poll() 的死循环，开始接收处理 IO 事件
    /* ---------------- 当外部调用 quit() 后，loop.loop() 才会退出到这里 ---------------- */
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr; // 6. 线程即将结束，清空指针
}