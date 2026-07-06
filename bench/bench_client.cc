// bench_client — 压测客户端（Task 17）
// 每连接一线程的阻塞 IO 客户端（压测端不做 Reactor，朴素拿数即可）：
//   connect → /nick → /join b<id%rooms> → 对齐统一起跑时刻 → 闭环收发：
//   发一条带 steady_clock 纳秒时间戳的消息，读行直到收到自己的回显再发下一条；
//   期间每收到一行都按行内时间戳记一个「投递延迟」样本（发出→送达）。
// 输出：发送/接收总数、投递吞吐 msg/s、延迟 p50/p99。
// --rooms 控制扇出：1 = 全员一房（广播串行化最严苛），R>1 = 均分进 R 个房间。
// 用法：bench_client [--conns 200] [--seconds 10] [--rooms 1] [--host 127.0.0.1] [--port 9000]
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

long nowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

// 每线程独立统计，跑完由主线程汇总，运行期间无共享（无锁无竞争）。
struct Stat {
  long sent = 0;
  long received = 0;
  std::vector<long> latencyNs;
};

void setRecvTimeout(int fd, int ms) {
  timeval tv{ms / 1000, (ms % 1000) * 1000};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

int connectTo(const std::string& host, uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0 ||
      ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

bool sendAll(int fd, const std::string& s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t n = ::send(fd, s.data() + off, s.size() - off, 0);
    if (n <= 0) return false;
    off += static_cast<size_t>(n);
  }
  return true;
}

// 阻塞 fd 上的逐行读取器：内部攒缓冲，半行留到下次拼完整。
struct LineReader {
  int fd;
  std::string buf;
  size_t pos = 0;
  bool readLine(std::string* line) {
    for (;;) {
      size_t nl = buf.find('\n', pos);
      if (nl != std::string::npos) {
        line->assign(buf, pos, nl - pos);
        pos = nl + 1;
        if (pos > (1u << 20)) {  // 回收已消费的前缀，防缓冲无限涨
          buf.erase(0, pos);
          pos = 0;
        }
        return true;
      }
      char tmp[4096];
      ssize_t n = ::recv(fd, tmp, sizeof tmp, 0);
      if (n <= 0) return false;  // 对端断开，或 SO_RCVTIMEO 超时
      buf.append(tmp, static_cast<size_t>(n));
    }
  }
};

void worker(int id, int rooms, const std::string& host, uint16_t port,
            long startNs, long endNs, Stat* st) {
  int fd = connectTo(host, port);
  if (fd < 0) {
    std::perror("connect");
    return;
  }
  const std::string nick = "u" + std::to_string(id);
  const std::string room = "b" + std::to_string(id % rooms);
  sendAll(fd, "/nick " + nick + "\n/join " + room + "\n");

  // 起跑前把欢迎语/加入广播全部丢掉，全体线程在 startNs 同时进入测量。
  setRecvTimeout(fd, 200);
  char junk[4096];
  while (nowNs() < startNs) {
    if (::recv(fd, junk, sizeof junk, 0) == 0) {  // 服务器断开
      ::close(fd);
      return;
    }
  }

  setRecvTimeout(fd, 5000);
  LineReader reader{fd, {}, 0};
  const std::string echoPrefix = "[" + room + "] " + nick + ": t";
  std::string line;
  st->latencyNs.reserve(1u << 15);
  bool alive = true;
  while (alive && nowNs() < endNs) {
    if (!sendAll(fd, "t" + std::to_string(nowNs()) + "\n")) break;
    ++st->sent;
    for (bool gotEcho = false; !gotEcho;) {  // 闭环：等到自己的回显再发下一条
      if (!reader.readLine(&line)) {
        alive = false;
        break;
      }
      const long recvT = nowNs();
      if (recvT >= endNs) {  // 到点，之后的行不再计入统计
        alive = false;
        break;
      }
      ++st->received;
      const size_t t = line.rfind(": t");  // 广播行形如 "[bench] uN: t<纳秒>"
      if (t != std::string::npos)
        st->latencyNs.push_back(recvT - std::atol(line.c_str() + t + 3));
      if (line.compare(0, echoPrefix.size(), echoPrefix) == 0) gotEcho = true;
    }
  }
  ::close(fd);
}

}  // namespace

int main(int argc, char* argv[]) {
  int conns = 200, seconds = 10, rooms = 1, port = 9000;
  std::string host = "127.0.0.1";
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string k = argv[i];
    if (k == "--conns") conns = std::atoi(argv[i + 1]);
    else if (k == "--seconds") seconds = std::atoi(argv[i + 1]);
    else if (k == "--rooms") rooms = std::atoi(argv[i + 1]);
    else if (k == "--host") host = argv[i + 1];
    else if (k == "--port") port = std::atoi(argv[i + 1]);
    else {
      std::fprintf(stderr,
                   "用法: %s [--conns N] [--seconds T] [--rooms R] [--host H] [--port P]\n",
                   argv[0]);
      return 1;
    }
  }
  if (rooms < 1) rooms = 1;
  // 3 秒准备窗口：留给 200 个线程 connect/join/清缓冲，然后同时起跑。
  const long startNs = nowNs() + 3'000'000'000L;
  const long endNs = startNs + seconds * 1'000'000'000L;

  std::vector<Stat> stats(conns);
  std::vector<std::thread> threads;
  threads.reserve(conns);
  for (int i = 0; i < conns; ++i)
    threads.emplace_back(worker, i, rooms, host, static_cast<uint16_t>(port),
                         startNs, endNs, &stats[i]);
  for (auto& t : threads) t.join();

  long sent = 0, received = 0;
  std::vector<long> lat;
  for (auto& s : stats) {
    sent += s.sent;
    received += s.received;
    lat.insert(lat.end(), s.latencyNs.begin(), s.latencyNs.end());
  }
  std::sort(lat.begin(), lat.end());
  auto pct = [&lat](double p) {
    return lat.empty() ? 0.0
                       : lat[static_cast<size_t>(p * (lat.size() - 1))] / 1e6;
  };
  std::printf("conns=%d rooms=%d seconds=%d\n", conns, rooms, seconds);
  std::printf("sent=%ld received=%ld throughput=%.0f msg/s (delivered)\n", sent,
              received, received / static_cast<double>(seconds));
  std::printf("latency(ms) p50=%.2f p99=%.2f  (%zu samples)\n", pct(0.50),
              pct(0.99), lat.size());
  return 0;
}
