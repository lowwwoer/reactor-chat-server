#include "net/EventLoop.h"
#include "net/EPollPoller.h"
#include "net/Channel.h"
#include "net/TimerQueue.h"
#include "base/Logging.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <chrono>

static int createEventfd() {
  int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (fd < 0) {
    LOG_SYSERR("eventfd");
    abort();
  }
  return fd;
}

EventLoop::EventLoop()
    : threadId_(std::this_thread::get_id()),
      poller_(std::make_unique<EPollPoller>(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_)) {
  // eventfd 也注册进 epoll：别的线程往它写 8 字节即可打断 epoll_wait。
  wakeupChannel_->setReadCallback([this] { handleWakeup(); });
  wakeupChannel_->enableReading();
  // poller_ 已就绪，可安全建 TimerQueue（其构造会把 timerfd 注册进本 loop 的 epoll）。
  timerQueue_ = std::make_unique<TimerQueue>(this);
}

EventLoop::~EventLoop() {
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
}

void EventLoop::loop() {
  quit_ = false;
  while (!quit_) {
    activeChannels_.clear();
    poller_->poll(10000 /*ms*/, &activeChannels_);
    for (Channel* ch : activeChannels_) ch->handleEvent();
    doPendingFunctors();  // 处理本轮排队的延迟任务
  }
}

void EventLoop::quit() {
  quit_ = true;
  // 跨线程 quit：loop 可能正阻塞在 epoll_wait，必须叫醒它才能看到 quit_。
  if (!isInLoopThread()) wakeup();
}

void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {
    cb();  // 已在属主线程：直接执行，零开销
  } else {
    queueInLoop(std::move(cb));
  }
}

void EventLoop::queueInLoop(Functor cb) {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    pendingFunctors_.push_back(std::move(cb));
  }
  // 两种情况必须唤醒：① 跨线程入队，loop 可能正阻塞；
  // ② 本线程但正在执行 pending 任务——新任务进不了本轮 swap 出来的批次，
  //    不唤醒的话要等下次 poll 超时才会执行。
  if (!isInLoopThread() || callingPendingFunctors_) wakeup();
}

void EventLoop::runAfter(double delaySeconds, Functor cb) {
  // 到期时刻在「调用时」就锁定为绝对时间点，跨线程投递路上的排队延迟不影响延时准确性。
  const auto when = std::chrono::steady_clock::now() +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(delaySeconds));
  // 堆与 timerfd 只归属主线程动，故经 runInLoop 转发：本线程直接登记，跨线程则排队 + 唤醒。
  runInLoop([this, when, cb = std::move(cb)]() mutable {
    timerQueue_->addTimer(when, std::move(cb));
  });
}

void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = ::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) LOG_SYSERR("EventLoop::wakeup write");
}

void EventLoop::handleWakeup() {
  uint64_t one;
  ssize_t n = ::read(wakeupFd_, &one, sizeof one);  // 清掉计数，避免反复触发
  if (n != sizeof one) LOG_SYSERR("EventLoop::handleWakeup read");
}

void EventLoop::doPendingFunctors() {
  // 先 swap 出来、锁外执行：① 缩短持锁时间；② 任务里可能再次 queueInLoop，
  // 若持锁执行会死锁；③ 避免遍历中容器被改动。
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;
  {
    std::lock_guard<std::mutex> lk(mutex_);
    functors.swap(pendingFunctors_);
  }
  for (auto& f : functors) f();
  callingPendingFunctors_ = false;
}

void EventLoop::updateChannel(Channel* ch) { poller_->updateChannel(ch); }
void EventLoop::removeChannel(Channel* ch) { poller_->removeChannel(ch); }
