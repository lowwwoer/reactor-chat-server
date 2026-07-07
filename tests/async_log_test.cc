// AsyncLogger 双缓冲异步日志测试（附录 A 拉伸项）：
// 多个前端线程并发 append 各自前缀的行，后端线程落盘；停止后校验
// ① 落盘总行数 = 线程数 × 每线程行数（后端不丢日志，含 stop 时的残留 flush）；
// ② 每行严格完整（并发写整行持锁拷贝，绝不撕裂/交错）。
#include "base/AsyncLogger.h"
#include "../tests/test_main.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

int main() {
  const std::string path = "/tmp/async_log_test.log";
  std::remove(path.c_str());
  const int kThreads = 4, kLines = 5000;

  {
    AsyncLogger log(path);
    log.start();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
      ts.emplace_back([&log, t] {
        for (int i = 0; i < kLines; ++i) {
          char buf[64];
          int n = std::snprintf(buf, sizeof buf, "T%02d-%06d-payload\n", t, i);
          log.append(buf, static_cast<size_t>(n));
        }
      });
    }
    for (auto& th : ts) th.join();
    log.stop();  // 落盘剩余并 join 后端线程
  }

  std::ifstream in(path);
  CHECK(in.good());
  std::unordered_map<int, int> perThread;
  std::string line;
  int total = 0;
  bool allWellFormed = true;
  while (std::getline(in, line)) {
    ++total;
    int tt = -1, ii = -1;
    char tail[16] = {0};
    // 撕裂/交错的行无法匹配这个定长格式，会把 allWellFormed 打成 false。
    if (std::sscanf(line.c_str(), "T%d-%d-%15s", &tt, &ii, tail) == 3 &&
        std::string(tail) == "payload") {
      perThread[tt]++;
    } else {
      allWellFormed = false;
    }
  }
  CHECK(allWellFormed);
  CHECK_EQ(total, kThreads * kLines);
  for (int t = 0; t < kThreads; ++t) CHECK_EQ(perThread[t], kLines);

  std::remove(path.c_str());
  TEST_SUMMARY();
}
