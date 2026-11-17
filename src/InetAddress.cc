#include <strings.h>
#include <string.h>

#include "InetAddress.h"

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    ::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET; // sin_family：地址族，IPv4固定为 AF_INET
    addr_.sin_port = ::htons(port); // 本地字节序转为网络字节序
    addr_.sin_addr.s_addr = ::inet_addr(ip.c_str()); // sin_addr：IPv4地址结构体（网络字节序）
}

std::string InetAddress::toIp() const
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // 将二进制网络字节序 IP 转回人类可读的点分十进制字符串，存放到buf中
    return buf;
}

std::string InetAddress::toIpPort() const
{
    // 格式化输出为 "ip:port"
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = ::strlen(buf);
    uint16_t port = ::ntohs(addr_.sin_port); // 网络字节序转回主机字节序
    sprintf(buf+end, ":%u", port); // %u：无符号十进制整数       buf+end 表示地址
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ::ntohs(addr_.sin_port); // 网络字节序转主机字节序
}

#if 0
#include <iostream>
int main()
{
    InetAddress addr(8080);
    std::cout << addr.toIpPort() << std::endl;
}
#endif