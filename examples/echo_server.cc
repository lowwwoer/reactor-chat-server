// 单线程 epoll echo 服务器：读到什么就原样写回。
// Phase 2 改用 TcpServer/TcpConnection 组装（对比 Phase 1 手撸裸 Channel 的版本，
// 业务侧只剩一个 messageCallback）。
// 监听 9000 端口，可用：  printf 'hi\n' | nc 127.0.0.1 9000  验证。
#include "net/TcpServer.h"
#include "net/EventLoop.h"
#include "base/Buffer.h"
#include "base/InetAddress.h"

int main() {
  EventLoop loop;
  TcpServer server(&loop, InetAddress(9000), "echo");
  server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf) {
    conn->send(buf->retrieveAllAsString());  // echo：读到啥写回啥
  });
  server.start();
  loop.loop();
}
