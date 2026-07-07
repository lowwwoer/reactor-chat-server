#include "base/AsyncLogger.h"
#include "base/Logging.h"

#include <chrono>
#include <cstdio>

AsyncLogger::AsyncLogger(std::string filename, int flushIntervalSec)
    : filename_(std::move(filename)),
      flushIntervalSec_(flushIntervalSec),
      currentBuffer_(std::make_unique<Buffer>()),
      nextBuffer_(std::make_unique<Buffer>()) {}

AsyncLogger::~AsyncLogger() { stop(); }

void AsyncLogger::append(const char* line, size_t len) {
  std::lock_guard<std::mutex> lk(mutex_);
  if (currentBuffer_->avail() >= len) {
    currentBuffer_->append(line, len);
  } else {
    // 当前缓冲装不下：挂到待写队列并唤醒后端，换一块空闲缓冲继续写。
    buffersToWrite_.push_back(std::move(currentBuffer_));
    // 优先用预备缓冲；后端还没来得及还回来时临时新建，绝不因缺缓冲而丢日志。
    currentBuffer_ = nextBuffer_ ? std::move(nextBuffer_) : std::make_unique<Buffer>();
    currentBuffer_->append(line, len);
    cond_.notify_one();
  }
}

void AsyncLogger::start() {
  // 无需加锁：running_=true 在建线程之前写，线程创建本身是 happens-before 边，
  // 后端线程一定看得到 true；此后 running_ 只由后端(读)与 stop(写)在持锁下访问。
  running_ = true;
  thread_ = std::thread([this] { threadFunc(); });
}

void AsyncLogger::stop() {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!running_) return;  // 幂等：未启动或已停止直接返回
    running_ = false;
  }
  cond_.notify_one();
  if (thread_.joinable()) thread_.join();
}

void AsyncLogger::threadFunc() {
  FILE* fp = std::fopen(filename_.c_str(), "a");
  if (!fp) {
    LOG_SYSERR("AsyncLogger fopen");
    return;
  }

  std::vector<BufferPtr> toWrite;
  bool stopping = false;
  while (!stopping) {
    {
      std::unique_lock<std::mutex> lk(mutex_);
      // 等到「有满缓冲 / 到刷新间隔 / 停止」三者之一；无论哪种醒来都往下走一轮落盘
      //（超时醒来则顺带 flush 未满的当前缓冲，限定日志延迟）。
      // 用 system_clock 的 wait_until 而非 steady_clock 的 wait_for：前者底层走
      // pthread_cond_timedwait(CLOCK_REALTIME)，老版 TSan 能正确拦截其锁释放；后者走
      // pthread_cond_clockwait(CLOCK_MONOTONIC)，gcc11 的 libtsan 漏拦会误报 double-lock。
      cond_.wait_until(
          lk, std::chrono::system_clock::now() + std::chrono::seconds(flushIntervalSec_),
          [this] { return !buffersToWrite_.empty() || !running_; });
      // 把当前缓冲（哪怕没写满）也纳入本轮落盘，以限定日志延迟；再换一块空闲缓冲给前端。
      buffersToWrite_.push_back(std::move(currentBuffer_));
      currentBuffer_ = nextBuffer_ ? std::move(nextBuffer_) : std::make_unique<Buffer>();
      toWrite.swap(buffersToWrite_);
      stopping = !running_;  // running_ 已置 false 时，前端线程必已全部结束（见 stop 的调用契约）
    }

    for (const auto& b : toWrite) {
      if (b->length() > 0) std::fwrite(b->data(), 1, b->length(), fp);
    }
    std::fflush(fp);

    // 复用一块写完的缓冲作为下次的预备缓冲，避免前端频繁分配（双缓冲的意义）。
    {
      std::lock_guard<std::mutex> lk(mutex_);
      if (!nextBuffer_ && !toWrite.empty()) {
        nextBuffer_ = std::move(toWrite.back());
        nextBuffer_->reset();
      }
    }
    toWrite.clear();
  }

  std::fclose(fp);
}
