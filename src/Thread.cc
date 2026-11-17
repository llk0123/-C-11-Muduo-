#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0); // 这统计的是创建过多少个 Thread 封装对象，不一定等于成功启动的系统线程数，因为对象可以构造后不调用 start()。

Thread::Thread(ThreadFunc func, const std::string &name) // 这里并不会立即创建线程，真正启动发生在 start()。
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func)) // 通过 std::move 减少函数对象的拷贝。     将参数中的函数对象移动到成员变量。
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();                                                  // thread类提供了设置分离线程的方法 线程运行后自动销毁（非阻塞）
    }
}

void Thread::start()                                                        // 一个Thread对象 记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;                                                              // 创建信号量
    sem_init(&sem, false, 0);                                               // 创建一个初值为 0 的进程内信号量    false 表示它不在进程间共享，供同一进程里的线程使用；     初值为 0，因此调用 sem_wait() 的线程会等待，直到另一个线程 sem_post()
    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        tid_ = CurrentThread::tid();                                        // 获取线程的tid值
        sem_post(&sem);                                                     // V 操作：信号量 +1，唤醒主线程       信号量加 1，通知调用 start() 的线程：“TID 已经初始化，可以读取了”
        func_();                                                            // 开启一个新线程 专门执行该线程函数    执行真正的业务回调
    }));

    // 主线程（调用 start 的线程）：阻塞等待新线程完成 TID 的赋值
    sem_wait(&sem);                                                         // P 操作：信号量 -1，如果值为 0 则阻塞。     调用 start() 的线程在这里等待。只有子线程写入 TID 并执行 sem_post() 后，它才能继续。     正常情况下 start() 返回时，tid_ 已经由子线程写好，调用方随后执行 tid() 可以读到有效 TID，而不是初始值 0。
} // 如果没有信号量等待，主线程调用完 start() 后可能会立刻去读取 thread.tid()，此时如果子线程还没来得及执行 tid_ = CurrentThread::tid()，主线程读到的就会是一个脏数据（默认值 0）。

// C++ std::thread 中join()和detach()的区别：https://blog.nowcoder.net/n/8fcd9bb6e2e94d9596cf0a45c8e5858a
void Thread::join() // 阻塞当前线程，直到绑定的那个子线程执行完毕并被回收。
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() // 先用原子变量 ++numCreated_ 拿到唯一递增序号。如果用户没给线程起名字（name_ 为空），则将其自动命名为 "Thread1"、"Thread2" 等，方便日志排查。
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
        // name_ = "Thread" + std::to_string(num); // 直接使用字符串拼接     其他方法 1


        //std::ostringstream oss;    // 其他方法 2      需要有 #include <sstream>
        //oss << "Thread" << num;
        //name_ = oss.str(); // 转换为 std::string 赋值
    }
}