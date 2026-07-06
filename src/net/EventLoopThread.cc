#include "net/EventLoopThread.h"
#include "net/EventLoop.h"

EventLoopThread::~EventLoopThread() {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (loop_) loop_->quit();  // 跨线程 quit：内部会 wakeup 叫醒 epoll_wait
  }
  // join 必须在锁外：IO 线程退出前要拿同一把锁把 loop_ 置空，持锁 join 会死锁。
  if (thread_.joinable()) thread_.join();
}

EventLoop* EventLoopThread::startLoop() {
  thread_ = std::thread([this] {
    EventLoop loop;  // loop 建在 IO 线程自己的栈上 —— one loop per thread
    {
      std::lock_guard<std::mutex> lk(mutex_);
      loop_ = &loop;
      cond_.notify_one();
    }
    loop.loop();  // 阻塞在此，直到 quit
    std::lock_guard<std::mutex> lk(mutex_);
    loop_ = nullptr;  // 先摘指针再让 loop 析构，析构函数里不会有人再用它
  });
  // 等 IO 线程把 loop 建好再返回，保证调用方拿到的指针立即可用。
  std::unique_lock<std::mutex> lk(mutex_);
  cond_.wait(lk, [this] { return loop_ != nullptr; });
  return loop_;
}
