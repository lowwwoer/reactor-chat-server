#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "base/InetAddress.h"

#include <unistd.h>

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingTCP()),
      acceptChannel_(loop, acceptSocket_.fd()) {
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.bindAddress(listenAddr);
  acceptChannel_.setReadCallback([this] { handleRead(); });
}

void Acceptor::listen() {
  acceptSocket_.listen();
  acceptChannel_.enableReading();  // 开始关注监听 fd 的可读事件
}

void Acceptor::handleRead() {
  // LT 模式下也循环 accept 到 EAGAIN：一次唤醒可能有多个连接到达，尽量收干净。
  while (true) {
    InetAddress peer(0);
    int connfd = acceptSocket_.accept(&peer);
    if (connfd >= 0) {
      if (newConnectionCallback_) {
        newConnectionCallback_(connfd, peer);
      } else {
        ::close(connfd);  // 没人接管就直接关，避免 fd 泄漏
      }
    } else {
      break;  // EAGAIN：本轮没有更多连接了
    }
  }
}
