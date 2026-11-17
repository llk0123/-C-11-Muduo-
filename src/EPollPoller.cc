#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

const int kNew = -1;    // 某个channel还没添加至Poller          // channel的成员index_初始化为-1
const int kAdded = 1;   // 某个channel已经添加至Poller
const int kDeleted = 2; // 某个channel已经从Poller删除

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC)) // 调用 ::epoll_create1 创建 epoll 句柄，EPOLL_CLOEXEC 标志防止子进程继承该 fd
    , events_(kInitEventListSize) // vector<epoll_event>(16)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno); // 如果创建失败，抛出致命日志并终止程序
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_); // 关闭 epollfd_ 这个数值所代表的 epoll 文件描述符（句柄），从而销毁内核中对应的 epoll 实例并释放相关系统资源。
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 由于频繁调用poll 实际上应该用LOG_DEBUG输出日志更为合理 当遇到并发场景 关闭DEBUG日志提升效率
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs); // &*events_.begin() 获取 vector 的底层原生数组首地址，把触发的事件写入 events_ 数组中
    int saveErrno = errno;
    Timestamp now(Timestamp::now()); 

    if (numEvents > 0) // 有 numEvents 个 fd 发生了读写/异常事件
    {
        LOG_INFO("%d events happend\n", numEvents); // LOG_DEBUG最合理
        fillActiveChannels(numEvents, activeChannels); // 将就绪事件打包转成 Channel 指针放进 activeChannels
        if (numEvents == events_.size()) // 扩容操作
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) // 超时，无事件发生
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else // 系统调用报错
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
void EPollPoller::updateChannel(Channel *channel) // 将 epoll 返回的就绪事件解析并填入 activeChannels
{
    const int index = channel->index(); // 获取该 channel 的状态
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) // 情况 A：新通道(kNew) 或 之前删过但重新启用的通道(kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        else // index == kDeleted
        {
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在Poller中注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 如果当前通道不再关注任何事件
        {
            update(EPOLL_CTL_DEL, channel); // 从 epoll 监听树上删除它
            channel->set_index(kDeleted); // 状态标记为 kDeleted（注意：并没有从 map 里删，随时可以重新 ADD）
        }
        else
        {
            update(EPOLL_CTL_MOD, channel); // 调用 epoll_ctl 执行 MOD 操作       具体进行什么修改，是根据channel中的event_知道的
        }
    }
}

// 从Poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的Poller给它返回的所有发生事件的channel列表了
    }
}

// 更新channel通道 其实就是调用epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    ::memset(&event, 0, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    //这里是联合体所以data.fd写了没有意义会被data.ptr覆盖
    // event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno); // 删除失败不会退出程序
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno); // 其他操作失败则会退出程序
        }
    }
}