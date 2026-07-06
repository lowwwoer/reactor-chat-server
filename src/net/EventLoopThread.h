#pragma once
// EventLoopThread：「一个线程 + 它拥有的那个 EventLoop」的绑定体。
// startLoop() 起线程，在新线程栈上建 EventLoop 并进入 loop()；
// 用条件变量等到 loop 真正建好后，把指针交回调用方（主 Reactor 线程）。
#include <condition_variable>
#include <mutex>
#include <thread>

class EventLoop;

class EventLoopThread {
 public:
  EventLoopThread() = default;
  ~EventLoopThread();  // 通知 loop 退出并 join，保证线程不泄漏

  EventLoopThread(const EventLoopThread&) = delete;
  EventLoopThread& operator=(const EventLoopThread&) = delete;

  EventLoop* startLoop();

 private:
  EventLoop* loop_ = nullptr;  // 指向 IO 线程栈上的 loop；线程退出后置回 nullptr
  std::thread thread_;
  std::mutex mutex_;           // 与 cond_ 配合保护 loop_ 的发布/回收
  std::condition_variable cond_;
};
