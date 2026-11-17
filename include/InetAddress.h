#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装socket地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1"); // 构造函数 1：指定端口和 IP（默认本机任意 IP "0.0.0.0"）
    explicit InetAddress(const sockaddr_in &addr) // 构造函数 2：直接传入底层的 sockaddr_in 结构体
        : addr_(addr)
    {
    }

    std::string toIp() const; // 获取格式化的 IP 字符串（例如 "127.0.0.1"）
    std::string toIpPort() const; // 获取格式化的 IP+Port 字符串（例如 "127.0.0.1:8080"）
    uint16_t toPort() const; // 获取端口号（主机字节序）

    const sockaddr_in *getSockAddr() const { return &addr_; } // 获取内部结构体指针（供 bind / connect / accept 等 C API 调用）
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_; // sockaddr_in：IPv4地址结构 16字节
};