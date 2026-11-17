#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

Socket::~Socket()
{
    ::close(sockfd_); // 利用 RAII 机制。当 Socket 对象生命周期结束（如离开作用域或被 delete）时，系统自动调用 close(sockfd_) 关闭对应的套接字，释放内核文件描述符资源。
}

void Socket::bindAddress(const InetAddress &localaddr) // 将当前套接字与指定的 IP 和端口（封装在 localaddr 中）绑定。若失败记录致命日志并退出。
{
    if (0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd:%d fail\n", sockfd_);
    }
}

void Socket::listen() // 将套接字转为被动监听状态
{
    if (0 != ::listen(sockfd_, 1024)) // 1024 是传入的 backlog 参数，代表 TCP 完全完成三次握手后等待 accept() 领走的已完成连接队列（Accept Queue）的最大长度。
    {
        LOG_FATAL("listen sockfd:%d fail\n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr) // 提取连接
{ // 传统 accept 拿到的 connfd 默认是阻塞的，如果需要非阻塞，还必须额外调用一次 fcntl(connfd, F_SETFL, O_NONBLOCK)，这涉及额外的系统调用开销。 这里使用 accept4
    /**
     * 1. accept函数的参数不合法
     * 2. 对返回的connfd没有设置非阻塞
     * Reactor模型 one loop per thread
     * poller + non-blocking IO
     **/
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ::memset(&addr, 0, sizeof(addr));
    // fixed : int connfd = ::accept(sockfd_, (sockaddr *)&addr, &len);
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC); // Linux 专有的 accept4 允许在提取连接的同时直接传入标志位 SOCK_NONBLOCK | SOCK_CLOEXEC
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() // 实现 TCP 的半关闭（Half-Close）
{ // close() 会同时关闭读写两端，并直接释放 fd；而 shutdown(..., SHUT_WR) 仅关闭写端（向对端发送 TCP FIN 包通知“我数据发完了”），但读端依然开启，仍能接收对端发来的数据。这是实现 TCP 优雅关闭的关键。
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite error");
    }
}

void Socket::setTcpNoDelay(bool on) // Nagle 算法会将多个小的数据包攒在一起凑成大包再发送，这会导致高延迟。
{
    // TCP_NODELAY 用于禁用 Nagle 算法。
    // Nagle 算法用于减少网络上传输的小数据包数量。
    // 将 TCP_NODELAY 设置为 1 可以禁用该算法，允许小数据包立即发送。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{ // 当服务器主动关闭连接后，TCP 端口会进入长达 1~2 分钟的 TIME_WAIT 状态，在此期间重新启动服务器进行 bind() 会提示 "Address already in use"。设置 SO_REUSEADDR 后允许套接字强行绑定处于 TIME_WAIT 状态的端口，实现服务器挂掉后立刻重启。
    // SO_REUSEADDR 允许一个套接字强制绑定到一个已被其他套接字使用的端口。
    // 这对于需要重启并绑定到相同端口的服务器应用程序非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{ // 允许多个独立的 Socket/进程/线程绑定完全相同的 IP 和端口。内核会在底层自动进行负载均衡，将新连接均匀分发给各个监听相同端口的进程/线程，解决了多进程/多线程同时 accept 同一个 listenfd 时的“惊群效应”
    // SO_REUSEPORT 允许同一主机上的多个套接字绑定到相同的端口号。
    // 这对于在多个线程或进程之间负载均衡传入连接非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    // SO_KEEPALIVE 启用在已连接的套接字上定期传输消息。
    // 如果另一端没有响应，则认为连接已断开并关闭。
    // 这对于检测网络中失效的对等方非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}