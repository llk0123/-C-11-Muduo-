#pragma once

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

#include "noncopyable.h"
// 对 C++11 标准库 std::thread 的封装，专门用来记录和管理一个工作线程的完整生命周期与元信息。
// 如果直接使用原生的 std::thread，我们很难方便地获取线程的真实真实内核 PID/TID、为线程设置自定义名称（如 Thread1），
// 或者优雅地处理析构时“既没 join 也没 detach 导致的程序崩溃”。这个类就是为了解决这些痛点而生的。
class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_; // 智能指针，用来延迟创建和托管真实的底层 C++ 线程对象。
    pid_t tid_;       // 在线程创建时再绑定      操作系统级别的真实线程 ID（内核 TID）
    ThreadFunc func_; // 线程回调函数
    std::string name_;
    static std::atomic_int numCreated_; // 统计创建了多少个线程   原子计数器
};