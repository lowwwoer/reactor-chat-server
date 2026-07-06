// 多人聊天服务器（主从 Reactor 多线程版，阶段三里程碑）。
// 协议：/nick <名> 设昵称，/join <房间> 进房，/who 看在线，/quit 退出，其余文本广播到当前房间。
// 监听 9000；可开多个   nc 127.0.0.1 9000   互相聊天演示。
// 环境变量：CHAT_ET=1 连接改用 ET 边沿触发（附录 A 拉伸项，默认 LT）。
#include "chat/ChatServer.h"
#include "net/EventLoop.h"
#include "base/InetAddress.h"
#include "base/Logging.h"

#include <cstdlib>

int main() {
  EventLoop loop;  // baseLoop：只管 accept
  ChatServer server(&loop, InetAddress(9000));
  server.setThreadNum(4);  // 4 个从 Reactor（IO 线程），连接轮询分发
  if (const char* et = std::getenv("CHAT_ET"); et && et[0] == '1') {
    server.setEdgeTriggered(true);
    LOG_INFO("ET mode enabled");
  }
  server.start();
  loop.loop();
}
