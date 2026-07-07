#pragma once
// EventLoop：事件循环。one loop per thread —— 每个线程恰好拥有一个 EventLoop。
// Phase 3 起支持跨线程投递：runInLoop 在本线程直接执行，否则排队并用 eventfd
// 唤醒阻塞中的 epoll_wait，让任务回到属主线程串行执行（见设计文档 §6）。
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <thread>

class Channel;
class EPollPoller;
class TimerQueue;

class EventLoop {
 public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  void loop();  // 事件循环：poll → 处理就绪事件 → 执行排队的任务
  void quit();  // 可跨线程调用：置位后若不在本线程则 wakeup 立刻退出

  // 跨线程安全投递：在属主线程则直接执行，否则排队 + 唤醒该 loop。
  // 这是整套并发设计的安全阀——别的线程永远不直接碰本 loop 的连接状态。
  void runInLoop(Functor cb);
  void queueInLoop(Functor cb);
  void wakeup();  // 往 eventfd 写 8 字节，叫醒阻塞中的 epoll_wait

  // delaySeconds 秒后在本 loop 线程执行 cb（附录 A 拉伸项，基于 timerfd）。
  // 可跨线程调用：内部走 runInLoop 把定时器登记回属主线程。到期时刻按调用时刻算。
  void runAfter(double delaySeconds, Functor cb);

  void updateChannel(Channel* ch);
  void removeChannel(Channel* ch);

  bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }

 private:
  void doPendingFunctors();
  void handleWakeup();  // 读掉 eventfd 计数，避免 LT 模式下反复触发

  std::atomic<bool> quit_{false};
  const std::thread::id threadId_;            // 创建本 loop 的线程
  std::unique_ptr<EPollPoller> poller_;
  std::vector<Channel*> activeChannels_;      // 每轮 poll 的就绪 Channel

  int wakeupFd_ = -1;                          // eventfd：跨线程唤醒
  std::unique_ptr<Channel> wakeupChannel_;
  std::unique_ptr<TimerQueue> timerQueue_;     // timerfd 定时器（runAfter 用）
  std::mutex mutex_;                           // 保护 pendingFunctors_（唯一跨线程入口）
  std::vector<Functor> pendingFunctors_;       // 待执行的延迟任务
  std::atomic<bool> callingPendingFunctors_{false};
};
