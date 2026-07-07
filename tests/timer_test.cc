// TimerQueue/EventLoop::runAfter 测试（附录 A 拉伸项）：
// 触发顺序按到期时间而非插入序（小根堆）、跨线程投递、回调内续约（踢空闲连接的用法）。
#include "net/EventLoop.h"
#include "../tests/test_main.h"
#include <chrono>
#include <thread>
#include <vector>

int main() {
  using Clock = std::chrono::steady_clock;
  EventLoop loop;
  // order/elapsed1 全部在 loop 线程读写（定时器回调在属主线程执行），无需加锁。
  std::vector<int> order;
  double elapsed1 = -1;
  const auto t0 = Clock::now();

  // 故意先加晚到期的，验证按到期时间排序
  loop.runAfter(0.3, [&] { order.push_back(2); });
  loop.runAfter(0.2, [&] {
    order.push_back(1);
    elapsed1 = std::chrono::duration<double>(Clock::now() - t0).count();
  });
  // 回调里再加定时器（踢空闲的「续约链」用法），最后一个负责 quit
  loop.runAfter(0.4, [&] {
    order.push_back(3);
    loop.runAfter(0.1, [&] {
      order.push_back(4);
      loop.quit();
    });
  });
  // 跨线程 runAfter：0.02s 远早于主线程那批，即使线程起得慢也最先触发
  std::thread other([&] { loop.runAfter(0.02, [&] { order.push_back(0); }); });

  loop.loop();
  other.join();

  CHECK_EQ(order.size(), 5u);
  for (int i = 0; i < 5; ++i) CHECK_EQ(order[i], i);
  CHECK(elapsed1 >= 0.2);  // timerfd 不会早于设定时刻触发
  CHECK(elapsed1 < 2.0);   // 上限放宽：TSan/CI 慢机留余量
  TEST_SUMMARY();
}
