#pragma once
// EventLoop：事件循环。one loop per thread —— 每个线程恰好拥有一个 EventLoop。
// 本阶段（Phase 1）是单线程版本；Phase 3 会加入 eventfd 唤醒与加锁，
// 让 runInLoop/queueInLoop 支持跨线程安全投递（见设计文档 §6）。
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <thread>

class Channel;
class EPollPoller;

class EventLoop {
 public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  void loop();  // 事件循环：poll → 处理就绪事件 → 执行排队的任务
  void quit();

  // 把任务排到「本轮事件处理结束后」执行。
  // 典型用途：在某个 Channel 的回调里，安全地销毁该 Channel 本身
  //（不能在回调里直接析构自己，只能推迟到回调返回之后）。
  void runInLoop(Functor cb);
  void queueInLoop(Functor cb);

  void updateChannel(Channel* ch);
  void removeChannel(Channel* ch);

  bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }

 private:
  void doPendingFunctors();

  std::atomic<bool> quit_{false};
  const std::thread::id threadId_;            // 创建本 loop 的线程
  std::unique_ptr<EPollPoller> poller_;
  std::vector<Channel*> activeChannels_;      // 每轮 poll 的就绪 Channel
  std::vector<Functor> pendingFunctors_;      // 待执行的延迟任务
};
