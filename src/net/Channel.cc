#include "net/Channel.h"
#include "net/EventLoop.h"

void Channel::update() { loop_->updateChannel(this); }
void Channel::remove() { loop_->removeChannel(this); }

void Channel::handleEvent() {
  if (tied_) {
    // 若绑定了对象，先 lock 成 shared_ptr 把它钉住，再处理事件，防止处理途中被析构。
    std::shared_ptr<void> guard = tie_.lock();
    if (guard) handleEventGuarded();
  } else {
    handleEventGuarded();
  }
}

void Channel::handleEventGuarded() {
  // 对端挂断（且没有可读数据）：当成关闭处理。
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) closeCallback_();
  }
  if (revents_ & EPOLLERR) {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
    if (readCallback_) readCallback_();
  }
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) writeCallback_();
  }
}
