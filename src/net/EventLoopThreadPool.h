#pragma once
// EventLoopThreadPool：从 Reactor 线程池。start() 起 N 个 EventLoopThread，
// getNextLoop() 轮询分发——主 Reactor 每 accept 一条新连接就从这里领一个 IO loop。
// N=0 时退化为单线程模式：所有连接都落在 baseLoop 上（阶段一/二的行为）。
#include <memory>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
 public:
  explicit EventLoopThreadPool(EventLoop* baseLoop);
  ~EventLoopThreadPool();

  EventLoopThreadPool(const EventLoopThreadPool&) = delete;
  EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

  void setThreadNum(int n) { numThreads_ = n; }
  void start();
  EventLoop* getNextLoop();  // 只在 baseLoop 线程调用（newConnection 里），无需加锁

 private:
  EventLoop* baseLoop_;  // 主 Reactor 的 loop（不属于本池，不负责其生命周期）
  int numThreads_ = 0;
  size_t next_ = 0;      // 轮询游标
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;  // 各 IO 线程的 loop（指向线程栈，随线程存亡）
};
