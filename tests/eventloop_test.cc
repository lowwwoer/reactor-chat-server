// EventLoop 跨线程投递测试：从别的线程 runInLoop 100 个任务 + quit，
// 验证任务全部回到 loop 线程执行、quit 能立刻唤醒阻塞中的 epoll_wait。
#include "net/EventLoop.h"
#include "../tests/test_main.h"
#include <thread>
#include <atomic>

int main() {
  EventLoop loop;
  std::atomic<int> hits{0};
  std::thread worker([&] {  // 从别的线程投递任务
    for (int i = 0; i < 100; ++i) loop.runInLoop([&] { ++hits; });
    loop.runInLoop([&] { loop.quit(); });
  });
  loop.loop();  // 在主线程跑
  worker.join();
  CHECK_EQ(hits.load(), 100);
  TEST_SUMMARY();
}
