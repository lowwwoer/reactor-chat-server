#pragma once
// TcpServer：把 Acceptor + 一堆 TcpConnection 组装成「一个服务器」的门面。
// 业务方只需 setMessageCallback / setConnectionCallback 再 start()，不碰 epoll 细节。
//
// Phase 3（Task 14）起为主从 Reactor：baseLoop 只 accept，新连接轮询分发到
// EventLoopThreadPool 里的 IO 线程；setThreadNum(0) 则退化回单线程（阶段二行为）。
#include <map>
#include <memory>
#include <string>
#include "base/InetAddress.h"
#include "net/TcpConnection.h"

class EventLoop;
class Acceptor;
class EventLoopThreadPool;

class TcpServer {
 public:
  TcpServer(EventLoop* loop, const InetAddress& addr, std::string name);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  void setThreadNum(int n) { threadNum_ = n; }  // 须在 start() 前调用
  // 连接 fd 改用 ET 边沿触发（附录 A 拉伸项），须在 start() 前调用。
  // 监听 fd 保持 LT：accept 循环本就取到 EAGAIN，且 LT 在 EMFILE 等瞬时失败后还会再通知。
  void setEdgeTriggered(bool on) { edgeTriggered_ = on; }
  void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
  void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

  void start();  // 启动 IO 线程池 + 开始监听

 private:
  void newConnection(int sockfd, const InetAddress& peer);  // Acceptor 回调进来
  void removeConnection(const TcpConnectionPtr& conn);       // 连接关闭时清理

  EventLoop* loop_;  // baseLoop / 主 Reactor
  const std::string name_;
  const InetAddress listenAddr_;  // 作为新连接的 local 地址传给 TcpConnection
  std::unique_ptr<Acceptor> acceptor_;
  std::unique_ptr<EventLoopThreadPool> threadPool_;  // 从 Reactor：IO 线程池

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  // 连接表只在 baseLoop 线程读写（newConnection / removeConnection 都回 baseLoop），无需加锁。
  std::map<std::string, TcpConnectionPtr> connections_;
  int nextConnId_ = 1;
  int threadNum_ = 0;
  bool edgeTriggered_ = false;
};
