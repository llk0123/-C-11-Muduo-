#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 **/
// 如果说 Acceptor 负责“迎客”（监听端口并接受新连接），那么 TcpConnection 就是负责“接待到底”：它代表一个已经建立成功的 TCP 客户端连接，全权管理该连接的生命周期、底层 Socket 读写、缓冲区管理以及各类业务回调的分发。
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection> // 允许对象在自身的成员函数中使用 shared_from_this 获取指向自己的 std::shared_ptr
{
public:
    TcpConnection(EventLoop *loop, // 当前连接所属的 EventLoop     注：TcpConnection 并不自己创建 EventLoop，而是由外部传入。
                  const std::string &nameArg, // 连接名称，通常用于日志和调试
                  int sockfd, // 已经建立连接的 TCP socket 文件描述符
                  const InetAddress &localAddr, // 本地 IP 地址和端口
                  const InetAddress &peerAddr); // 对端客户端 IP 地址和端口
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; } // 返回当前连接所属的事件循环。
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; } // 判断当前连接是否处于已连接状态。  这里涉及下面的状态机 enum StateE

    // 发送数据
    void send(const std::string &buf); // 用户向 TCP 连接发送字符串数据     send() 返回时，数据不一定已经到达对端，甚至不一定已经全部进入本机内核。  会先写入内核，如果内核满了就写入用户缓冲区，注册EPOLLOUT
    void sendFile(int fileDescriptor, off_t offset, size_t count); // 发送文件，也可选择发送其中的一部分。    fd：文件描述符  offset：从文件的哪个字节开始发送  count：发送的字节数
    
    // 关闭半连接（半关闭）    关闭写方向
    void shutdown();
    // 回调函数设计
    void setConnectionCallback(const ConnectionCallback &cb)
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb)
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback &cb)
    { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) // 高水位回调  当输出缓冲区中的待发送数据达到指定阈值时就触发高水回调，具体操作得看回调函数
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    // 连接建立
    void connectEstablished(); // 完成 TcpConnection 从创建到正式投入事件循环管理的过程
    // 连接销毁
    void connectDestroyed(); // 负责连接销毁过程中的清理工作。

private:
    enum StateE
    {
        kDisconnected, // 已经断开连接
        kConnecting,   // 正在连接
        kConnected,    // 已连接
        kDisconnecting // 正在断开连接
    };
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime); // Timestamp receiveTime：记录这次 I/O 事件对应的接收时间
    void handleWrite();//处理写事件
    void handleClose();
    void handleError();

    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
    EventLoop *loop_; // 表示当前 TcpConnection 所属的事件循环     这里使用普通指针，而不是：std::shared_ptr<EventLoop>，这通常意味着 TcpConnection 并不拥有 EventLoop 的生命周期。
    const std::string name_;
    std::atomic_int state_;
    bool reading_;//连接是否在监听读事件 

    // Socket Channel 这里和Acceptor类似    Acceptor => mainloop    TcpConnection => subloop
    std::unique_ptr<Socket> socket_; // 一个 TcpConnection 独占管理一个 Socket 对象
    std::unique_ptr<Channel> channel_; // 一个TcpConnection 独占管理一个 Channel 对象

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 这些回调TcpServer也有 用户通过写入TcpServer注册 TcpServer再将注册的回调传递给TcpConnection TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调
    CloseCallback closeCallback_; // 关闭连接的回调
    size_t highWaterMark_; // 高水位阈值

    // 数据缓冲区
    Buffer inputBuffer_;    // 接收数据的缓冲区
    Buffer outputBuffer_;   // 发送数据的缓冲区 用户send向outputBuffer_发
};
