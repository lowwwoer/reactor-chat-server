#pragma once
// 监听/连接 socket 的 RAII 封装：析构即 close(fd)，杜绝 fd 泄漏。
// Socket 拥有 fd（负责关闭）；Channel 只是 fd 上的事件分发器（不拥有）。
#include "base/InetAddress.h"

class Socket {
 public:
  explicit Socket(int fd) : fd_(fd) {}
  ~Socket();
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  int fd() const { return fd_; }
  void bindAddress(const InetAddress& addr);
  void listen();
  int accept(InetAddress* peer);   // 成功返回非阻塞 connfd；无连接(EAGAIN)返回 -1
  void setReuseAddr(bool on);
  void shutdownWrite();            // 半关闭：只关写端

 private:
  const int fd_;
};

namespace sockets {
int createNonblockingTCP();        // 建非阻塞 IPv4 TCP socket
void setNonBlocking(int fd);
}  // namespace sockets
