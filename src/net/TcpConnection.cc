#include "net/TcpConnection.h"
#include "net/EventLoop.h"
#include "base/Logging.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

TcpConnection::TcpConnection(EventLoop* loop, std::string name, int sockfd,
                             const InetAddress& local, const InetAddress& peer)
    : loop_(loop),
      name_(std::move(name)),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      localAddr_(local),
      peerAddr_(peer) {
  // 四类事件分别绑到本连接的处理函数。事件触发期间由 Channel::tie 钉住本对象，
  // 所以这里用裸 [this] 捕获是安全的（见 connectEstablished 里的 channel_->tie）。
  channel_->setReadCallback([this] { handleRead(); });
  channel_->setWriteCallback([this] { handleWrite(); });
  channel_->setCloseCallback([this] { handleClose(); });
  channel_->setErrorCallback([this] { handleError(); });
}

void TcpConnection::connectEstablished() {
  state_ = kConnected;
  channel_->tie(shared_from_this());  // weak_ptr 钉住生命周期，保护事件处理期
  channel_->enableReading();          // 注册到 epoll，开始关注可读
  if (connectionCallback_) connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
  // TcpServer 把连接移出连接表后调用，做最终清理。
  if (state_ == kConnected) {  // 服务器主动关闭路径：handleClose 还没走过
    state_ = kDisconnected;
    channel_->disableAll();
    if (connectionCallback_) connectionCallback_(shared_from_this());
  }
  channel_->remove();  // 从 Poller 注销本 channel
}

void TcpConnection::handleRead() {
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    // 把可读数据交给上层；inputBuffer_ 里可能是半包，由上层（Codec）决定取多少。
    if (messageCallback_) messageCallback_(shared_from_this(), &inputBuffer_);
  } else if (n == 0) {
    handleClose();  // 对端正常关闭
  } else {
    errno = savedErrno;
    handleError();
  }
}

void TcpConnection::send(const std::string& msg) {
  if (state_ != kConnected) return;
  if (loop_->isInLoopThread()) {
    sendInLoop(msg);
  } else {
    // 跨线程（典型：别的 IO 线程在广播）：把写操作投递回属主线程。
    // 捕获 shared_from_this() 保证任务执行时连接对象还活着。
    loop_->runInLoop([self = shared_from_this(), m = msg] { self->sendInLoop(m); });
  }
}

void TcpConnection::sendInLoop(const std::string& msg) {
  // 投递与执行之间连接可能已断开，进属主线程后再查一次状态。
  if (state_ != kConnected) return;

  ssize_t nwrote = 0;
  size_t remaining = msg.size();
  // 输出缓冲为空且当前没在等可写：直接写一把，省掉一次拷贝与一轮 epoll 往返。
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(), msg.data(), msg.size());
    if (nwrote >= 0) {
      remaining = msg.size() - nwrote;
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) LOG_SYSERR("TcpConnection::sendInLoop");
    }
  }
  // 没一次写完：剩余进输出缓冲，开始关注可写事件，等内核可写时由 handleWrite 续写。
  if (remaining > 0) {
    outputBuffer_.append(msg.data() + nwrote, remaining);
    if (!channel_->isWriting()) channel_->enableWriting();
  }
}

void TcpConnection::handleWrite() {
  if (!channel_->isWriting()) return;
  ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(),
                      outputBuffer_.readableBytes());
  if (n > 0) {
    outputBuffer_.retrieve(n);
    if (outputBuffer_.readableBytes() == 0) {
      channel_->disableWriting();  // 写空了就别再关注可写，否则 epoll 会一直空转
      if (state_ == kDisconnecting) shutdownInLoop();  // 之前请求过关闭：flush 完再关
    }
  } else {
    LOG_SYSERR("TcpConnection::handleWrite");
  }
}

void TcpConnection::shutdown() {
  if (state_ == kConnected) {
    state_ = kDisconnecting;
    // shutdownWrite 动的是 channel/socket 状态，必须回属主线程执行。
    loop_->runInLoop([self = shared_from_this()] { self->shutdownInLoop(); });
  }
}

void TcpConnection::shutdownInLoop() {
  if (!channel_->isWriting()) {
    // 输出缓冲已 flush 完，安全地半关闭写端：对端 read 会读到 EOF。
    socket_->shutdownWrite();
  }
  // 否则还有数据没发完，等 handleWrite 把缓冲写空后会再调本函数。
}

void TcpConnection::handleClose() {
  state_ = kDisconnected;
  channel_->disableAll();
  TcpConnectionPtr guard(shared_from_this());  // 钉住自己，回调里安全
  if (connectionCallback_) connectionCallback_(guard);  // 通知上层「断开了」
  if (closeCallback_) closeCallback_(guard);            // → TcpServer::removeConnection
}

void TcpConnection::handleError() {
  // 取 SO_ERROR 把真实错误码打出来；连接的回收仍走 handleClose / removeConnection。
  int optval = 0;
  socklen_t optlen = sizeof optval;
  if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    optval = errno;
  }
  LOG_ERROR("TcpConnection::handleError [%s] SO_ERROR=%d", name_.c_str(), optval);
}
