#pragma once

#include <string>

#include "noncopyable.h"
// 实现了一个轻量级的日志打印系统，用于在网络库运行过程中格式化输出日志信息。
// LOG_INFO("%s %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \ // do { ... } while(0) 结构： 编写多行宏定义时的标准技巧。避免在 if-else 嵌套等控制流中使用宏时发生语法歧义。
    {                                                     \
        Logger &logger = Logger::instance();              \ // instance()：通过局部静态变量 static Logger logger; 确保内存中仅存在一个全局日志实例。
        logger.setLogLevel(INFO);                         \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \ // ##__VA_ARGS__ 可以智能处理当可变参数为空时自动抹除前面的逗号，防止语法错误。
        logger.log(buf);                                  \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(ERROR);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0)

#ifdef MUDEBUG // 只有在定义了 MUDEBUG 宏时才会编译 LOG_DEBUG 语句
#define LOG_DEBUG(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(DEBUG);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

// 定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core dump信息
    DEBUG, // 调试信息
};

// 输出一个日志类

class Logger : noncopyable
{
public:
    // 获取日志唯一的实例对象 单例
    static Logger &instance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(std::string msg);

private:
    int logLevel_;
};