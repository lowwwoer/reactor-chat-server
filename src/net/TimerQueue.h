#pragma once
// TimerQueue：基于 timerfd 的定时器集合（附录 A 拉伸项）。所有定时器共用一个 timerfd，
// 内部用小根堆按到期时间排序，timerfd 永远指向「最早到期」的那个；到期时 epoll 报可读，
// 一次性触发全部已到期回调（含回调内新加的定时器——踢空闲连接的「续约链」即靠此）。
// 只在属主 EventLoop 线程访问，故无锁；跨线程添加请走 EventLoop::runAfter（内部 runInLoop）。
#include <chrono>
#include <functional>
#include <queue>
#include <vector>
#include "net/Channel.h"

class EventLoop;

class TimerQueue {
 public:
  using Functor = std::function<void()>;
  using TimePoint = std::chrono::steady_clock::time_point;

  explicit TimerQueue(EventLoop* loop);
  ~TimerQueue();

  TimerQueue(const TimerQueue&) = delete;
  TimerQueue& operator=(const TimerQueue&) = delete;

  // 在 when 到期时执行 cb。必须在属主 loop 线程调用（由 EventLoop::runAfter 保证）。
  void addTimer(TimePoint when, Functor cb);

 private:
  struct Timer {
    TimePoint when;
    Functor cb;
  };
  // 小根堆：priority_queue 默认大顶堆，用「when 更晚者优先级更低」的比较翻成小顶，堆顶即最早到期。
  struct LaterFirst {
    bool operator()(const Timer& a, const Timer& b) const { return a.when > b.when; }
  };

  void handleRead();                  // timerfd 可读：触发所有已到期定时器
  void resetTimerfd(TimePoint when);  // 重设 timerfd 指向最早到期时刻

  EventLoop* loop_;
  const int timerfd_;
  Channel timerfdChannel_;
  std::priority_queue<Timer, std::vector<Timer>, LaterFirst> timers_;
};
