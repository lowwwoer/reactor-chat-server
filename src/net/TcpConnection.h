#pragma once
// TcpConnection：一条 TCP 连接的「生命周期 + 收发」封装，是网络库面向业务的核心对象。
// 用 shared_ptr 管理：连接随时可能断开，而断开那一刻可能正处在它自己的回调里，
// 靠 Channel::tie 存的 weak_ptr 在事件处理期间把对象「钉住」，避免 use-after-free。
//
// Phase 3（Task 14）起 send/shutdown 跨线程安全：不在属主 loop 线程时，
// 用 loop_->runInLoop 把实际操作投递回属主线程串行执行（广播即依赖此保证）。
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include "base/Buffer.h"
#include "base/InetAddress.h"
#include "base/Socket.h"
#include "net/Channel.h"

class EventLoop;

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
// 业务回调：网络层只搬字节，通过这三类回调把「连接建立/断开」「收到数据」交给上层。
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
 public:
  TcpConnection(EventLoop* loop, std::string name, int sockfd,
                const InetAddress& local, const InetAddress& peer);

  TcpConnection(const TcpConnection&) = delete;
  TcpConnection& operator=(const TcpConnection&) = delete;

  void send(const std::string& msg);  // 任意线程可调：自动转发到属主 loop
  void shutdown();                     // 半关闭写端（输出缓冲 flush 完才真正 shutdown）

  void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
  void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
  void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

  // 由 TcpServer 在连接刚建立 / 即将销毁时调用（都在属主 loop 线程）。
  void connectEstablished();  // 注册到 epoll + 触发 connectionCallback
  void connectDestroyed();    // 从 epoll 注销 + 最终清理

  EventLoop* getLoop() const { return loop_; }
  const std::string& name() const { return name_; }
  bool connected() const { return state_ == kConnected; }

  // 给业务挂一个不透明的会话对象（聊天里就是 Session{nick, room}）。
  void setContext(const std::shared_ptr<void>& c) { context_ = c; }
  std::shared_ptr<void> getContext() const { return context_; }

 private:
  enum StateE { kConnecting, kConnected, kDisconnecting, kDisconnected };

  void handleRead();
  void handleWrite();
  void handleClose();
  void handleError();
  void sendInLoop(const std::string& msg);  // 真正的写逻辑，只在属主线程执行
  void shutdownInLoop();

  EventLoop* loop_;
  const std::string name_;
  // atomic：state_ 只由属主线程写，但 send()/connected() 可能从别的线程读
  //（广播路径），原子读写避免数据竞争（TSan 验证）。
  std::atomic<StateE> state_{kConnecting};

  std::unique_ptr<Socket> socket_;    // 拥有 connfd，析构即 close
  std::unique_ptr<Channel> channel_;  // connfd 上的事件分发器
  const InetAddress localAddr_;
  const InetAddress peerAddr_;

  Buffer inputBuffer_;   // 收：readFd 读进来，交给 messageCallback
  Buffer outputBuffer_;  // 发：一次没写完的剩余，等可写事件由 handleWrite 续写

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  CloseCallback closeCallback_;
  std::shared_ptr<void> context_;
};
