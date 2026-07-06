#include "net/TcpServer.h"
#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"

#include <string>

TcpServer::TcpServer(EventLoop* loop, const InetAddress& addr, std::string name)
    : loop_(loop),
      name_(std::move(name)),
      listenAddr_(addr),
      acceptor_(std::make_unique<Acceptor>(loop, addr)),
      threadPool_(std::make_unique<EventLoopThreadPool>(loop)) {
  // accept 到新连接后回调进 newConnection（运行在 baseLoop 线程）。
  acceptor_->setNewConnectionCallback(
      [this](int sockfd, const InetAddress& peer) { newConnection(sockfd, peer); });
}

TcpServer::~TcpServer() = default;

void TcpServer::start() {
  threadPool_->setThreadNum(threadNum_);
  threadPool_->start();  // 先把 IO 线程全部拉起，再开门迎客
  loop_->runInLoop([this] { acceptor_->listen(); });
}

void TcpServer::newConnection(int sockfd, const InetAddress& peer) {
  std::string connName = name_ + "#" + std::to_string(nextConnId_++);
  // 主从 Reactor 的分发点：从线程池轮询领一个 IO loop，这条连接终生归它管。
  EventLoop* ioLoop = threadPool_->getNextLoop();
  auto conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, listenAddr_, peer);
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  // 连接关闭时回到这里把它从连接表摘掉（closeCallback 由 TcpConnection::handleClose 触发）。
  conn->setCloseCallback([this](const TcpConnectionPtr& c) { removeConnection(c); });
  // 注册 epoll 必须发生在连接的属主线程，不能在 baseLoop 线程直接做。
  ioLoop->runInLoop([conn] { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
  // closeCallback 在连接的 IO 线程触发，而 connections_ 只归 baseLoop 管：回 baseLoop 改表。
  loop_->runInLoop([this, conn] {
    connections_.erase(conn->name());
    // 再回连接自己的 IO 线程做最终清理。不能就地销毁 Channel（可能正处在
    // 它自己的事件回调里），queueInLoop 推迟到该 loop 本轮事件处理结束后。
    // lambda 按值捕获 conn，把它的生命周期续到 connectDestroyed 执行完。
    conn->getLoop()->queueInLoop([conn] { conn->connectDestroyed(); });
  });
}
