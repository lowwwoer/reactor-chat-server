#pragma once
// TcpServer：把 Acceptor + 一堆 TcpConnection 组装成「一个服务器」的门面。
// 业务方只需 setMessageCallback / setConnectionCallback 再 start()，不碰 epoll 细节。
//
// 本阶段（Phase 2 / Task 9）：单线程——所有连接都建在 baseLoop 上。
// Phase 3（Task 13）会接入从 Reactor 线程池，把新连接轮询分发到各 IO 线程。
#include <map>
#include <memory>
#include <string>
#include "base/InetAddress.h"
#include "net/TcpConnection.h"

class EventLoop;
class Acceptor;

class TcpServer {
 public:
  TcpServer(EventLoop* loop, const InetAddress& addr, std::string name);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  void setThreadNum(int n) { threadNum_ = n; }  // Task 13 生效，本任务先存值
  void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
  void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

  void start();  // 开始监听

 private:
  void newConnection(int sockfd, const InetAddress& peer);  // Acceptor 回调进来
  void removeConnection(const TcpConnectionPtr& conn);       // 连接关闭时清理

  EventLoop* loop_;  // baseLoop / 主 Reactor
  const std::string name_;
  const InetAddress listenAddr_;  // 作为新连接的 local 地址传给 TcpConnection
  std::unique_ptr<Acceptor> acceptor_;

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  std::map<std::string, TcpConnectionPtr> connections_;  // 连接名 → 连接
  int nextConnId_ = 1;
  int threadNum_ = 0;
};
