#pragma once
// 极简日志宏：够用即可，不做异步/分级落盘（可选拉伸项再升级）。
#include <cstdio>
#include <cstring>
#include <cerrno>

#define LOG_INFO(...)                       \
  do {                                      \
    std::fprintf(stderr, "[INFO] ");        \
    std::fprintf(stderr, __VA_ARGS__);      \
    std::fprintf(stderr, "\n");             \
  } while (0)

#define LOG_ERROR(...)                      \
  do {                                      \
    std::fprintf(stderr, "[ERROR] ");       \
    std::fprintf(stderr, __VA_ARGS__);      \
    std::fprintf(stderr, "\n");             \
  } while (0)

// 打印「自定义消息 + errno 对应的系统错误字符串」。
#define LOG_SYSERR(msg) LOG_ERROR("%s: %s", msg, std::strerror(errno))
