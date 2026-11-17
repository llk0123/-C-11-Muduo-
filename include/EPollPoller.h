#pragma once

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"

/**
 * epoll的使用:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;  // 执行 epoll_wait 监听   timeoutMs：超时时间
    void updateChannel(Channel *channel) override; // 更新 Channel 的 epoll 关注状态
    void removeChannel(Channel *channel) override; // 从 epoll 中彻底注销 Channel

private:
    static const int kInitEventListSize = 16; // 默认给事件数组分配的初始容量大小

    // 填写活跃的连接       1：把 epoll_wait 返回的就绪事件打包填入 activeChannels
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道 其实就是调用epoll_ctl      2：封装底层系统的 epoll_ctl 调用（ADD / MOD / DEL）
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>; // C++中可以省略struct 直接写epoll_event即可

    int epollfd_;      // epoll_create创建返回的fd保存在epollfd_中
    EventList events_; // 用于存放epoll_wait返回的所有发生的事件的文件描述符事件集
};