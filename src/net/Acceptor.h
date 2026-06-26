#pragma once
// Acceptor：活在主 Reactor 上，专管监听 socket。可读即 accept，
// 把新 connfd 通过回调交给上层（Phase 1 直接由 echo 示例处理，
// Phase 3 起由 TcpServer 分发给某个从 Reactor）。
#include <functional>
#include "base/Socket.h"
#include "net/Channel.h"

class EventLoop;
class InetAddress;

class Acceptor {
 public:
  using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr);

  void setNewConnectionCallback(NewConnectionCallback cb) {
    newConnectionCallback_ = std::move(cb);
  }
  void listen();

 private:
  void handleRead();  // 监听 fd 可读：循环 accept 直到 EAGAIN

  EventLoop* loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
};
