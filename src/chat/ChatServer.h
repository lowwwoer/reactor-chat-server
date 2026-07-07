#pragma once
// ChatServer：聊天业务层。只关心「连接来了/走了」和「收到一行文本怎么处理」，
// 完全不碰 epoll —— 网络细节都委托给内部的 TcpServer。
//
// 命令：/nick <名> 设昵称，/join <房间> 进房，/who 看在线，/quit 退出；
// 其余文本广播到当前房间。本阶段单线程；Task 15 给 RoomManager 加锁后才支持多线程。
#include "base/InetAddress.h"
#include "chat/RoomManager.h"
#include "net/TcpServer.h"

class EventLoop;
class Buffer;

class ChatServer {
 public:
  ChatServer(EventLoop* loop, const InetAddress& addr);

  void setThreadNum(int n);        // 透传给内部 TcpServer（Task 13 生效）
  void setEdgeTriggered(bool on);  // 透传：连接改用 ET（附录 A 拉伸项）
  // 空闲超时秒数：连接超过该时长无输入即被踢（附录 A 拉伸项）。0=关闭，须在 start() 前设。
  void setIdleTimeout(double seconds) { idleTimeout_ = seconds; }
  void start();

 private:
  void onConnection(const TcpConnectionPtr& conn);
  void onMessage(const TcpConnectionPtr& conn, Buffer* buf);
  // 在 conn 的属主 loop 上延时 delay 秒后查一次空闲：仍活跃则按剩余时间续约，否则踢。
  void armIdleTimer(const TcpConnectionPtr& conn, double delay);

  TcpServer server_;
  RoomManager rooms_;
  double idleTimeout_ = 0;  // 0=不启用空闲踢除
};
