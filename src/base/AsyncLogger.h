#pragma once
// AsyncLogger：双缓冲异步日志（附录 A 拉伸项）。前端多线程调 append，把整行持锁拷进
// 当前缓冲（故行与行之间绝不撕裂）；缓冲写满就挂到待写队列、唤醒后端，前端换一块空闲
// 缓冲继续写，几乎不被磁盘 IO 阻塞。后端线程 swap 出待写队列在锁外落盘；停止时把残留
// 缓冲也 flush 完再退出，保证不丢日志。这是经典的「前端环形/双缓冲 + 后端落盘」结构。
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class AsyncLogger {
 public:
  explicit AsyncLogger(std::string filename, int flushIntervalSec = 3);
  ~AsyncLogger();

  AsyncLogger(const AsyncLogger&) = delete;
  AsyncLogger& operator=(const AsyncLogger&) = delete;

  void append(const char* line, size_t len);  // 前端：任意线程可调
  void start();  // 拉起后端落盘线程
  void stop();   // 通知后端 flush 剩余日志并 join（幂等）

 private:
  // 定长缓冲：一段连续内存 + 写偏移，append 直接把整行 memcpy 到尾部。
  // 单行长度须小于 kSize（日志行一般几十~几百字节，远小于 64KB）。
  class Buffer {
   public:
    Buffer() : data_(kSize), cur_(0) {}
    size_t avail() const { return kSize - cur_; }
    size_t length() const { return cur_; }
    const char* data() const { return data_.data(); }
    void append(const char* buf, size_t len) {
      std::memcpy(data_.data() + cur_, buf, len);
      cur_ += len;
    }
    void reset() { cur_ = 0; }

   private:
    static const size_t kSize = 64 * 1024;
    std::vector<char> data_;
    size_t cur_;
  };
  using BufferPtr = std::unique_ptr<Buffer>;

  void threadFunc();  // 后端线程主体

  const std::string filename_;
  const int flushIntervalSec_;

  std::mutex mutex_;
  std::condition_variable cond_;
  bool running_ = false;
  BufferPtr currentBuffer_;                // 前端正在写的缓冲
  BufferPtr nextBuffer_;                    // 预备的空闲缓冲（双缓冲的第二块）
  std::vector<BufferPtr> buffersToWrite_;  // 已写满、待后端落盘的缓冲
  std::thread thread_;
};
