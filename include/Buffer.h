#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;//初始预留的prependabel空间大小
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(kCheapPrepend + initalSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; } // Readable 区域 (readerIndex_ ~ writerIndex_)   当前尚未被应用层读取的数据内容
    size_t writableBytes() const { return buffer_.size() - writerIndex_; } // Writable 区域 (writerIndex_ ~ buffer_.size())   剩余可供追加写入数据的空闲空间
    size_t prependableBytes() const { return readerIndex_; } // Prependable 区域(0 ~readerIndex_)，预留的前缀空间，初始大小为 kCheapPrepend = 8 字节。以在已知数据前高效添加包头（如包长度），而无需重新分配内存和移动全部数据。

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const { return begin() + readerIndex_; }
    void retrieve(size_t len) // 修改读取len长度后的 readerInder_ 位置
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 说明应用只读取了可读缓冲区数据的一部分，就是len长度 还剩下readerIndex+=len到writerIndex_的数据未读
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }
    void retrieveAll() // 读取了缓冲区中所有内容，修改readerIndex_、writerIndex_为初始的
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size - writerIndex_
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容
        }
    }

    // 把[data, data+len)内存上的数据添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len); // 堆上开辟新空间
        std::copy(data, data+len, beginWrite()); // 数据从栈拷贝到堆
        writerIndex_ += len; // 更新指针与自动清理     writerIndex_ += len 移动写指针，标记堆内存上的数据增加。随后 readFd 函数执行完毕，栈上的 extrabuf 自动销毁
    }
    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // vector底层数组首元素的地址 也就是数组的起始地址
    char *begin() { return &*buffer_.begin(); }
    const char *begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len)
    {
        /**
         * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
         * | kCheapPrepend | reader ｜          len          |
         **/
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) // 也就是说 len > xxx前面剩余的空间 + writer的部分     情况 1：剩余的 writable 空间 + 前面已废弃的 prependable 空间都不够装下 len
        {
            buffer_.resize(writerIndex_ + len);
        }
        else // 这里说明 len <= xxx + writer 把reader搬到从xxx开始 使得xxx后面是一段连续空间     情况 2：整体空闲空间是够的，只是前面的已读数据（碎片）占了位置
        {
            size_t readable = readableBytes(); // readable = reader的长度
            // 将当前缓冲区中从readerIndex_到writerIndex_的数据
            // 拷贝到缓冲区起始位置kCheapPrepend处，以便腾出更多的可写空间
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
