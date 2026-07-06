#include "net/EventLoopThreadPool.h"
#include "net/EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop) : baseLoop_(baseLoop) {}

// 析构放 .cc：头文件里 EventLoopThread 只有前置声明，unique_ptr 析构需要完整类型。
// 各 EventLoopThread 析构时自会 quit + join 自己的线程。
EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::start() {
  for (int i = 0; i < numThreads_; ++i) {
    threads_.push_back(std::make_unique<EventLoopThread>());
    loops_.push_back(threads_.back()->startLoop());  // 阻塞到该 IO 线程的 loop 建好
  }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
  if (loops_.empty()) return baseLoop_;  // 没开 IO 线程：连接全落在 baseLoop
  EventLoop* loop = loops_[next_];
  next_ = (next_ + 1) % loops_.size();
  return loop;
}
