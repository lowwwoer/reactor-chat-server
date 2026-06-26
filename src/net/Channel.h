#pragma once
// Channel：一个 fd 上「关注哪些事件 + 事件来了调哪个回调」的分发器。
// 注意：Channel 不拥有 fd（不负责 close），fd 的关闭由 Socket / 上层负责。
// 一个 Channel 终生只属于一个 EventLoop。
#include <functional>
#include <memory>
#include <sys/epoll.h>

class EventLoop;

class Channel {
 public:
  using EventCallback = std::function<void()>;

  Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd) {}

  void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  // 把 Channel 的生命周期「绑」到某个 shared_ptr 上（通常是 TcpConnection 自己）。
  // 事件处理期间 lock 成 shared_ptr 把对象钉住，防止回调执行到一半对象被析构。
  void tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
  }

  int fd() const { return fd_; }
  int events() const { return events_; }
  void set_revents(int revt) { revents_ = revt; }

  void enableReading() { events_ |= kReadEvent; update(); }
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  void disableAll() { events_ = kNoneEvent; update(); }
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  // 给 EPollPoller 记录该 fd 在 epoll 中的状态（kNew/kAdded/kDeleted）。
  int index() const { return index_; }
  void set_index(int i) { index_ = i; }

  EventLoop* ownerLoop() { return loop_; }

  void handleEvent();  // epoll 返回后由 EventLoop 调用，按 revents_ 派发回调
  void remove();       // 从所属 EventLoop / Poller 中移除自己

 private:
  void update();  // → loop_->updateChannel(this)
  void handleEventGuarded();

  static const int kNoneEvent = 0;
  static const int kReadEvent = EPOLLIN | EPOLLPRI;
  static const int kWriteEvent = EPOLLOUT;

  EventLoop* loop_;
  const int fd_;
  int events_ = 0;    // 关注的事件
  int revents_ = 0;   // epoll 返回的、实际就绪的事件
  int index_ = -1;    // = kNew，Poller 用

  bool tied_ = false;
  std::weak_ptr<void> tie_;

  EventCallback readCallback_, writeCallback_, closeCallback_, errorCallback_;
};
