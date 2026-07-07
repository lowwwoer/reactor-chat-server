#include "net/TimerQueue.h"
#include "net/EventLoop.h"
#include "base/Logging.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

static int createTimerfd() {
  // CLOCK_MONOTONIC：和 steady_clock 同源，只用相对时长下发，不受系统时钟调整影响。
  int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd < 0) {
    LOG_SYSERR("timerfd_create");
    abort();
  }
  return fd;
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop, timerfd_) {
  // timerfd 也挂进 epoll：到期即可读，回到属主线程处理定时器（和普通 IO 事件同一条路径）。
  timerfdChannel_.setReadCallback([this] { handleRead(); });
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
}

void TimerQueue::addTimer(TimePoint when, Functor cb) {
  // 新定时器若比现有最早的还早，必须把 timerfd 往前调，否则会错过它。
  const bool earliest = timers_.empty() || when < timers_.top().when;
  timers_.push(Timer{when, std::move(cb)});
  if (earliest) resetTimerfd(when);
}

void TimerQueue::resetTimerfd(TimePoint when) {
  const auto now = std::chrono::steady_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(when - now).count();
  if (ns < 1000) ns = 1000;  // 至少 1us：it_value 全 0 会「停掉」timerfd，已到期的也要给个极小正值立刻触发
  itimerspec its;
  std::memset(&its, 0, sizeof its);
  its.it_value.tv_sec = ns / 1000000000;
  its.it_value.tv_nsec = ns % 1000000000;  // it_interval 保持 0 → 一次性定时器
  if (::timerfd_settime(timerfd_, 0, &its, nullptr) < 0) LOG_SYSERR("timerfd_settime");
}

void TimerQueue::handleRead() {
  uint64_t howmany;
  ssize_t n = ::read(timerfd_, &howmany, sizeof howmany);  // 读掉计数，避免 LT 下反复触发
  if (n != sizeof howmany) LOG_SYSERR("TimerQueue::handleRead");

  // 先把所有已到期的摘出来再逐个执行：回调里可能再 addTimer（续约），
  // 就地遍历堆会被并发修改；摘出来后新加的定时器进入下一轮，逻辑清晰。
  const TimePoint now = std::chrono::steady_clock::now();
  std::vector<Timer> expired;
  while (!timers_.empty() && timers_.top().when <= now) {
    expired.push_back(timers_.top());
    timers_.pop();
  }
  for (Timer& t : expired) t.cb();

  // 回调可能已重设过 timerfd；这里再按当前堆顶兜底重设一次，保证指向真正最早到期者。
  if (!timers_.empty()) resetTimerfd(timers_.top().when);
}
