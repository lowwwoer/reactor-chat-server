// 多人聊天服务器（主从 Reactor 多线程版，阶段三里程碑）。
// 协议：/nick <名> 设昵称，/join <房间> 进房，/who 看在线，/quit 退出，其余文本广播到当前房间。
// 监听 9000；可开多个   nc 127.0.0.1 9000   互相聊天演示。
#include "chat/ChatServer.h"
#include "net/EventLoop.h"
#include "base/InetAddress.h"

int main() {
  EventLoop loop;  // baseLoop：只管 accept
  ChatServer server(&loop, InetAddress(9000));
  server.setThreadNum(4);  // 4 个从 Reactor（IO 线程），连接轮询分发
  server.start();
  loop.loop();
}
