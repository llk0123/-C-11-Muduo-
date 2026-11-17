#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP); // 创建非阻塞、CLOEXEC 的 TCP 套接字
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking()) // 1. 创建底层 listenfd
    , acceptChannel_(loop, acceptSocket_.fd()) // 2. 将 listenfd 包装为 Channel
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true); // 开启地址复用
    acceptSocket_.setReusePort(true); // 开启端口复用（多线程/多进程惊群优化）
    acceptSocket_.bindAddress(listenAddr); // 绑定 IP 和端口
    // TcpServer::start() => Acceptor.listen() 如果有新用户连接 要执行一个回调(accept => connfd => 打包成Channel => 唤醒subloop)
    // baseloop监听到有事件发生 => acceptChannel_(listenfd) => 执行该回调函数
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();    // 把从Poller中感兴趣的事件删除掉       停止关注所有 IO 事件
    acceptChannel_.remove();        // 调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
}

void Acceptor::listen() // 启动监听
{
    listenning_ = true;
    acceptSocket_.listen();         // 调用底层 ::listen()，让 listenfd 进入被动监听状态
    acceptChannel_.enableReading(); // 将 acceptChannel_ 的读事件注册到 Poller（激活 epoll_ctl ADD）
}

// listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr); // 调用 ::accept4 接收新客户端连接
    if (connfd >= 0)
    {
        if (NewConnectionCallback_)
        { // 触发回调：把接收到的新连接 fd 以及对端 IP 地址传给 TcpServer
            NewConnectionCallback_(connfd, peerAddr); // 轮询找到subLoop 唤醒并分发当前的新客户端的Channel
        }
        else
        {
            ::close(connfd); // 如果应用层没注册回调，无法处理该连接，直接关掉
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) // 文件描述符耗尽异常
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}