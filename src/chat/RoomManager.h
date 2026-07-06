#pragma once
// RoomManager：房间表 —— 哪个房间里有哪些连接。聊天里唯一的跨线程共享可变状态，
// 用一把 mutex 保护（设计文档 §6.4）。多个 IO 线程会并发调 join/broadcast：
// 广播持锁遍历成员逐个 conn->send —— send 只是把写投递回属主 loop（非阻塞、很快），
// 持锁时间短，成员变更不频繁，锁竞争可控。
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "net/TcpConnection.h"

// 每条连接挂一个 Session（存在 TcpConnection 的 context 里）。
struct Session {
  std::string nick;
  std::string room;  // 当前所在房间；空串表示还没 /join
};

class RoomManager {
 public:
  void join(const std::string& room, const TcpConnectionPtr& conn);
  void leave(const TcpConnectionPtr& conn);
  void broadcast(const std::string& room, const std::string& line);
  std::vector<std::string> members(const std::string& room);

 private:
  mutable std::mutex mutex_;  // 保护 rooms_（四个公有方法都可能被不同 IO 线程调）
  std::unordered_map<std::string, std::set<TcpConnectionPtr>> rooms_;
};
