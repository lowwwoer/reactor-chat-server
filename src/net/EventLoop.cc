#include "net/EventLoop.h"
#include "net/EPollPoller.h"
#include "net/Channel.h"

EventLoop::EventLoop()
    : threadId_(std::this_thread::get_id()),
      poller_(std::make_unique<EPollPoller>(this)) {}

EventLoop::~EventLoop() = default;

void EventLoop::loop() {
  quit_ = false;
  while (!quit_) {
    activeChannels_.clear();
    poller_->poll(10000 /*ms*/, &activeChannels_);
    for (Channel* ch : activeChannels_) ch->handleEvent();
    doPendingFunctors();  // 处理本轮排队的延迟任务
  }
}

void EventLoop::quit() { quit_ = true; }

void EventLoop::runInLoop(Functor cb) {
  // 单线程阶段：调用方一定就在 loop 线程，直接执行即可。
  // Phase 3 起会变成：在本线程则直接执行，否则投递到目标 loop。
  cb();
}

void EventLoop::queueInLoop(Functor cb) {
  pendingFunctors_.push_back(std::move(cb));
  // Phase 3 起：若从别的线程入队，这里需要 wakeup() 叫醒阻塞中的 epoll_wait。
}

void EventLoop::doPendingFunctors() {
  // 先 swap 出来再执行：避免执行任务期间又有新任务入队，导致遍历的容器被改动。
  std::vector<Functor> functors;
  functors.swap(pendingFunctors_);
  for (auto& f : functors) f();
}

void EventLoop::updateChannel(Channel* ch) { poller_->updateChannel(ch); }
void EventLoop::removeChannel(Channel* ch) { poller_->removeChannel(ch); }
