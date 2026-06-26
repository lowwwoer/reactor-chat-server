#include "base/Socket.h"
#include "base/Logging.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <csignal>
#include <cstdlib>
#include <cstring>

namespace {
// 进程启动即忽略 SIGPIPE：向已关闭的连接写数据时，默认会收到 SIGPIPE 把进程杀掉；
// 忽略后 write 改为返回 EPIPE，由我们自己处理。用一个全局对象在 main 之前完成设置。
struct IgnoreSigPipe {
  IgnoreSigPipe() { ::signal(SIGPIPE, SIG_IGN); }
} g_ignoreSigPipe;
}  // namespace

Socket::~Socket() { ::close(fd_); }

void Socket::bindAddress(const InetAddress& addr) {
  if (::bind(fd_, reinterpret_cast<const sockaddr*>(addr.sockAddr()),
             sizeof(sockaddr_in)) < 0) {
    LOG_SYSERR("Socket::bindAddress");
    abort();  // 监听地址绑定失败属致命错误，直接退出
  }
}

void Socket::listen() {
  if (::listen(fd_, SOMAXCONN) < 0) {
    LOG_SYSERR("Socket::listen");
    abort();
  }
}

int Socket::accept(InetAddress* peer) {
  sockaddr_in addr;
  socklen_t len = sizeof addr;
  std::memset(&addr, 0, sizeof addr);
  // accept4 一步拿到非阻塞 + CLOEXEC 的 connfd，省去额外 fcntl。
  int connfd = ::accept4(fd_, reinterpret_cast<sockaddr*>(&addr), &len,
                         SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (connfd >= 0 && peer) *peer = InetAddress(addr);
  return connfd;
}

void Socket::setReuseAddr(bool on) {
  int opt = on ? 1 : 0;
  ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
}

void Socket::shutdownWrite() {
  if (::shutdown(fd_, SHUT_WR) < 0) LOG_SYSERR("Socket::shutdownWrite");
}

namespace sockets {

int createNonblockingTCP() {
  int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  if (fd < 0) {
    LOG_SYSERR("sockets::createNonblockingTCP");
    abort();
  }
  return fd;
}

void setNonBlocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

}  // namespace sockets
