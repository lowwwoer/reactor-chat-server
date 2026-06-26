#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <sys/types.h>

class Buffer {
public:
  static const size_t kPrepend = 8;
  static const size_t kInitial = 1024;
  explicit Buffer(size_t initial = kInitial)
      : buf_(kPrepend + initial), readerIndex_(kPrepend), writerIndex_(kPrepend) {}

  size_t readableBytes() const { return writerIndex_ - readerIndex_; }
  size_t writableBytes() const { return buf_.size() - writerIndex_; }
  size_t prependableBytes() const { return readerIndex_; }

  const char* peek() const { return begin() + readerIndex_; }

  void retrieve(size_t len) {
    if (len < readableBytes()) readerIndex_ += len;
    else retrieveAll();
  }
  void retrieveAll() { readerIndex_ = writerIndex_ = kPrepend; }
  std::string retrieveAsString(size_t len) {
    std::string s(peek(), len); retrieve(len); return s;
  }
  std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
  void retrieveUntil(const char* end) { retrieve(end - peek()); }

  void append(const char* data, size_t len) {
    ensureWritable(len);
    std::copy(data, data + len, beginWrite());
    writerIndex_ += len;
  }
  void append(const std::string& s) { append(s.data(), s.size()); }

  const char* findEOL() const {
    const void* eol = std::memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(eol);
  }

  // 用 readv：先填可写区，溢出部分进 64KB 栈上 extrabuf，再 append 回来。
  ssize_t readFd(int fd, int* savedErrno);

private:
  char* begin() { return buf_.data(); }
  const char* begin() const { return buf_.data(); }
  char* beginWrite() { return begin() + writerIndex_; }

  void ensureWritable(size_t len) {
    if (writableBytes() < len) makeSpace(len);
  }
  void makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kPrepend) {
      buf_.resize(writerIndex_ + len);
    } else {                                  // 把已读空间回收，数据前移
      size_t readable = readableBytes();
      std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kPrepend);
      readerIndex_ = kPrepend;
      writerIndex_ = readerIndex_ + readable;
    }
  }

  std::vector<char> buf_;
  size_t readerIndex_;
  size_t writerIndex_;
};
