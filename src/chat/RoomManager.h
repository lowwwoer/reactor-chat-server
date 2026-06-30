#pragma once
// RoomManager：房间表 —— 哪个房间里有哪些连接。聊天里唯一的共享可变状态。
// 本阶段单线程，不加锁；Phase 4（Task 15）会用 mutex 保护，支持多 IO 线程并发访问。
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
  std::unordered_map<std::string, std::set<TcpConnectionPtr>> rooms_;
};
