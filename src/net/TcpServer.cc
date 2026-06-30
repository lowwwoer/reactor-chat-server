#include "net/TcpServer.h"
#include "net/Acceptor.h"
#include "net/EventLoop.h"

#include <string>

TcpServer::TcpServer(EventLoop* loop, const InetAddress& addr, std::string name)
    : loop_(loop),
      name_(std::move(name)),
      listenAddr_(addr),
      acceptor_(std::make_unique<Acceptor>(loop, addr)) {
  // accept 到新连接后回调进 newConnection（运行在 baseLoop 线程）。
  acceptor_->setNewConnectionCallback(
      [this](int sockfd, const InetAddress& peer) { newConnection(sockfd, peer); });
}

TcpServer::~TcpServer() = default;

void TcpServer::start() {
  acceptor_->listen();  // 开始监听；有连接到达就触发 newConnection
}

void TcpServer::newConnection(int sockfd, const InetAddress& peer) {
  std::string connName = name_ + "#" + std::to_string(nextConnId_++);
  // 本任务单线程：连接就建在 baseLoop 上（从 Reactor 线程池在 Task 13 接入）。
  auto conn = std::make_shared<TcpConnection>(loop_, connName, sockfd, listenAddr_, peer);
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  // 连接关闭时回到这里把它从连接表摘掉（closeCallback 由 TcpConnection::handleClose 触发）。
  conn->setCloseCallback([this](const TcpConnectionPtr& c) { removeConnection(c); });
  conn->connectEstablished();
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
  connections_.erase(conn->name());
  // 不能在事件回调里就地销毁连接的 Channel；用 queueInLoop 推迟到本轮事件处理结束后再清理。
  // lambda 按值捕获 conn，把它的生命周期续到 connectDestroyed 执行完。
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
}
