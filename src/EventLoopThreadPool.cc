#include <memory>

#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "Logger.h"
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // Don't delete loop, it's stack variable
    // 析构函数为空的原因：
    // 1. threads_ 里的 std::unique_ptr<EventLoopThread> 会自动析构并销毁所有子线程。
    // 2. loops_ 中存放的 EventLoop* 指针都是由对应的 EventLoopThread 线程在自己的栈空间上创建的，不需要也不应该由线程池来 delete。
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) // 循环创建指定数量的 EventLoopThread 线程
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i); // 为线程生成前缀名称（如 poolName0, poolName1）
        EventLoopThread *t = new EventLoopThread(cb, buf); // 创建 IO 线程对象
        threads_.push_back(std::unique_ptr<EventLoopThread>(t)); // 纳入智能指针接管
        loops_.push_back(t->startLoop()); // 底层创建线程 绑定一个新的EventLoop 并返回该loop的地址     startLoop 会阻塞等待子线程将 EventLoop 创建完毕，并返回该子线程的 EventLoop* 指针
    }

    if (numThreads_ == 0 && cb) // 整个服务端只有一个线程运行baseLoop
    {
        cb(baseLoop_); // 直接在 baseLoop_ 上触发初始化回调
    }
}

// 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
EventLoop *EventLoopThreadPool::getNextLoop()
{
    // 如果只设置一个线程 也就是只有一个mainReactor 无subReactor 
    // 那么轮询只有一个线程 getNextLoop()每次都返回当前的baseLoop_
    EventLoop *loop = baseLoop_;    

    // 通过轮询获取下一个处理事件的loop
    // 如果没设置多线程数量，则不会进去，相当于直接返回baseLoop
    if(!loops_.empty())             
    {
        loop = loops_[next_]; // 取出当前索引位置的 SubLoop 指针
        ++next_;
        // 轮询
        if(next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}



std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    { // 只有单线程模式（没有创建子线程），返回只包含 baseLoop_ 的 vector
        return std::vector<EventLoop *>(1, baseLoop_); // 调用的是 std::vector 的重载构造函数：std::vector<T>(count, value)  count表示vector的初始大小
    }
    else
    {
        return loops_; // 返回包含所有工作线程 SubLoop 指针的数组
    }
}