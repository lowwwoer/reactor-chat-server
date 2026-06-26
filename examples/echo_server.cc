// 单线程 epoll echo 服务器：读到什么就原样写回。
// Phase 1 里直接用「裸 Channel」管理每个连接（TcpConnection 是 Phase 2 的事）。
// 监听 9000 端口，可用：  printf 'hi\n' | nc 127.0.0.1 9000  验证。
#include "net/EventLoop.h"
#include "net/Acceptor.h"
#include "net/Channel.h"
#include "base/Buffer.h"
#include "base/InetAddress.h"

#include <map>
#include <memory>
#include <string>
#include <unistd.h>

int main() {
  EventLoop loop;
  Acceptor acceptor(&loop, InetAddress(9000));

  // connfd → 它的 Channel。Channel 不拥有 fd，连接关闭时我们手动 close 再移除。
  static std::map<int, std::unique_ptr<Channel>> conns;

  acceptor.setNewConnectionCallback([&loop](int fd, const InetAddress& peer) {
    (void)peer;
    auto ch = std::make_unique<Channel>(&loop, fd);
    Channel* raw = ch.get();

    raw->setReadCallback([&loop, fd, raw] {
      Buffer buf;
      int savedErrno = 0;
      ssize_t n = buf.readFd(fd, &savedErrno);
      if (n > 0) {
        std::string s = buf.retrieveAllAsString();
        ::write(fd, s.data(), s.size());  // echo
      } else {
        // n==0 对端关闭；n<0 出错。都按关闭处理。
        raw->disableAll();  // 先从 epoll 注销关注
        raw->remove();      // 再从 Poller 表里移除
        ::close(fd);        // 关闭 fd（Channel 不负责 close）
        // 关键：不能在 Channel 自己的回调里析构自己，
        // 推迟到本轮事件处理结束后再删（见 EventLoop::queueInLoop）。
        loop.queueInLoop([fd] { conns.erase(fd); });
      }
    });

    raw->enableReading();
    conns[fd] = std::move(ch);
  });

  acceptor.listen();
  loop.loop();
}
